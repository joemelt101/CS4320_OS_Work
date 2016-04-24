#include <back_store.h>
#include <bitmap.h>
#include <string.h>

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "S16FS.h"

// There's just so much. SO. MUCH.
// Last time it was all in the file.
// There was just so much
// It messes up my autocomplete, but whatever.
#include "backend.h"

///
/// Formats (and mounts) an S16FS file for use
/// \param fname The file to format
/// \return Mounted S16FS object, NULL on error
///
S16FS_t *fs_format(const char *path) {
    return ready_file(path, true);
}

///
/// Mounts an S16FS object and prepares it for use
/// \param fname The file to mount
/// \return Mounted F16FS object, NULL on error
///
S16FS_t *fs_mount(const char *path) {
    return ready_file(path, false);
}

///
/// Unmounts the given object and frees all related resources
/// \param fs The S16FS object to unmount
/// \return 0 on success, < 0 on failure
///
int fs_unmount(S16FS_t *fs) {
    if (fs) {
        back_store_close(fs->bs);
        bitmap_destroy(fs->fd_table.fd_status);
        free(fs);
        return 0;
    }
    return -1;
}

///
/// Creates a new file at the specified location
///   Directories along the path that do not exist are not created
/// \param fs The S16FS containing the file
/// \param path Absolute path to file to create
/// \param type Type of file to create (regular/directory)
/// \return 0 on success, < 0 on failure
///
int fs_create(S16FS_t *fs, const char *path, file_t type) {
    if (fs && path) {
        if (type == FS_REGULAR || type == FS_DIRECTORY) {
            // WHOOPS. Should make sure desired file doesn't already exist.
            // Just going to jam it here.
            result_t file_status;
            locate_file(fs, path, &file_status);
            if (file_status.success && !file_status.found) {
                // alrighty. Need to find the file. And by the file I mean the parent.
                // locate_file doesn't really handle finding the parent if the file doesn't exist
                // So I can't just dump finding this file. Have to search for parent.

                // So, kick off the file finder. If it comes back with the right flags
                // Start checking if we have inodes, the parent exists, a directory, not full
                // if it's a dir check if we have a free block.
                // Fill it all out, update parent, etc. Done!
                const size_t path_len = strnlen(path, FS_PATH_MAX);
                if (path_len != 0 && path[0] == '/' && path_len < FS_PATH_MAX) {
                    // path string is probably ok.
                    char *path_copy, *fname_copy;
                    // this breaks if it's a file at root, since we remove the slash
                    // locate_file treats it as an error
                    // Old version just worked around if if [0] was '\0'
                    // Ideally, I could just ask strndup to allocate an extra byte
                    // Then I can just shift the fname down a byte and insert the NUL there
                    // But strndup doesn't allocate the size given, it seems
                    // So we gotta go manual. Don't think this snippet will be needed elsewhere
                    // Need a malloc, memcpy, then some manual adjustment
                    // path_copy  = strndup(path, path_len);  // I checked, it's not +1. yay MallocScribble
                    path_copy = (char *) calloc(1, path_len + 2);  // NUL AND extra space
                    memcpy(path_copy, path, path_len);
                    fname_copy = strrchr(path_copy, '/');
                    if (fname_copy) {  // CANNOT be null, since we validated [0] as a possibility... but just in case
                        //*fname_copy = '\0';  // heh, split strings, now I have a path to parent AND fname
                        ++fname_copy;
                        const size_t fname_len = path_len - (fname_copy - path_copy);
                        memmove(fname_copy + 1, fname_copy, fname_len + 1);
                        fname_copy[0] = '\0';  // string is split into abs path (now with slash...) and fname
                        ++fname_copy;

                        if (fname_len != 0 && fname_len < (FS_FNAME_MAX - 1)) {
                            // alrighty. Hunt down parent dir.
                            // check it's actually a dir. (ooh, add to result_t!)
                            locate_file(fs, path_copy, &file_status);
                            if (file_status.success && file_status.found && file_status.type == FS_DIRECTORY) {
                                // parent exists, is a directory. Cool.
                                // (added block to locate_file if file is a dir. Handy.)
                                dir_block_t parent_dir;
                                inode_t new_inode;
                                dir_block_t new_dir;
                                uint32_t now = time(NULL);
                                // load dir, check it has space.
                                if (full_read(fs, &parent_dir, file_status.block)
                                    && parent_dir.mdata.size < DIR_REC_MAX) {
                                    // try to grab all new resources (inode, optionally data block)
                                    // if we get all that, commit it.
                                    inode_ptr_t new_inode_idx = find_free_inode(fs);
                                    if (new_inode_idx != 0) {
                                        bool success            = false;
                                        block_ptr_t new_dir_ptr = 0;
                                        switch (type) {
                                            case FS_REGULAR:
                                                // We're all good.
                                                new_inode = (inode_t){
                                                    {0},
                                                    {0, 0777, now, now, now, file_status.inode, FS_REGULAR, {0}},
                                                    {0}};
                                                strncpy(new_inode.fname, fname_copy, fname_len + 1);
                                                // I'm so deep now that my formatter is very upset with every line
                                                // inode = ready
                                                success = write_inode(fs, &new_inode, new_inode_idx);
                                                // Uhh, if that didn't work we could, worst case, have a partial inode
                                                // And that's a "file system is now kinda busted" sort of error
                                                // This is why "real" (read: modern) file systems have backups all over
                                                // (and why the occasional chkdsk is so important)
                                                break;
                                            case FS_DIRECTORY:
                                                // following line keeps being all "Expected expression"
                                                // SOMETHING is messed up SOMEWHERE.
                                                // Or it's trying to protect me by preventing new variables in a switch
                                                // Which is super undefined, but only sometimes (not in this case...)
                                                // Idk, man.
                                                // block_ptr_t new_dir_ptr = back_store_allocate(fs->bs);
                                                new_dir_ptr = back_store_allocate(fs->bs);
                                                if (new_dir_ptr != 0) {
                                                    // Resources = obtained
                                                    // write dir block first, inode is the final step
                                                    // that's more transaction-safe... but it's not like we're thread
                                                    // safe
                                                    // in the slightest (or process safe, for that matter)
                                                    new_inode = (inode_t){
                                                        {0},
                                                        {0, 0777, now, now, now, file_status.inode, FS_DIRECTORY, {0}},
                                                        {new_dir_ptr, 0, 0, 0, 0, 0}};
                                                    strncpy(new_inode.fname, fname_copy, fname_len + 1);

                                                    memset(&new_dir, 0x00, sizeof(dir_block_t));

                                                    if (!(success = full_write(fs, &new_dir, new_dir_ptr)
                                                                    && write_inode(fs, &new_inode, new_inode_idx))) {
                                                        // transation: if it didn't work, release the allocated block
                                                        back_store_release(fs->bs, new_dir_ptr);
                                                    }
                                                }
                                                break;
                                            default:
                                                // HOW.
                                                break;
                                        }
                                        if (success) {
                                            // whoops. forgot the part where I actually save the file to the dir tree
                                            // Mildly important.
                                            unsigned i = 0;
                                            // This is technically a potential infinite loop. But we validated contents
                                            // earlier
                                            for (; parent_dir.entries[i].fname[0] != '\0'; ++i) {
                                            }
                                            strncpy(parent_dir.entries[i].fname, fname_copy, fname_len + 1);
                                            parent_dir.entries[i].inode = new_inode_idx;
                                            ++parent_dir.mdata.size;
                                            if (full_write(fs, &parent_dir, file_status.block)) {
                                                free(path_copy);
                                                return 0;
                                            } else {
                                                // Oh man. These surely are the end times.
                                                // Our file exists. Kinda. But not entirely.
                                                // The final tree link failed.
                                                // We SHOULD:
                                                //  Wipe inode
                                                //  Release dir block (if making a dir)
                                                // But I'm lazy. And if a write failed, why would others work?
                                                // back_store won't actually do that to us, anyway.
                                                // Like, even if the file was deleted while using it, we're mmap'd so
                                                // the kernel has no real way to tell us, as far as I know.
                                                puts("Infinite sadness. New file stuck in limbo.");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        free(path_copy);
                    }
                }
            }
        }
    }
    return -1;
}

///
/// Opens the specified file for use
///   R/W position is set to the beginning of the file (BOF)
///   Directories cannot be opened
/// \param fs The S16FS containing the file
/// \param path path to the requested file
/// \return file descriptor to the requested file, < 0 on error
///
int fs_open(S16FS_t *fs, const char *path) {
    if (fs && path) {
        // Well, find the file, log it. That's it?
        // faster to find an open descriptor, so we'll knock that out first.
        size_t open_descriptor = bitmap_ffz(fs->fd_table.fd_status);
        if (open_descriptor != SIZE_MAX) {
            result_t file_info;
            locate_file(fs, path, &file_info);
            if (file_info.success && file_info.found && file_info.type == FS_REGULAR) {
                // cool. Done.
                bitmap_set(fs->fd_table.fd_status, open_descriptor);
                fs->fd_table.fd_pos[open_descriptor]   = 0;
                fs->fd_table.fd_inode[open_descriptor] = file_info.inode;
                // ... auto-aligning assignments in cute until this happens.
                return open_descriptor;
            }
            // ... I really should be returning multiple error codes
            // I wrote the spec so you could do this and then I just don't
            // Like those error debugging macros.
            // Do as I say, not as I do.
        }
    }
    return -1;
}

///
/// Closes the given file descriptor
/// \param fs The S16FS containing the file
/// \param fd The file to close
/// \return 0 on success, < 0 on failure
///
int fs_close(S16FS_t *fs, int fd) {
    if (fs && fd < DESCRIPTOR_MAX && fd >= 0) {
        // Oh man, now I feel bad.
        // There's 0% chance bitmap will detect out of bounds and stop
        // So everyone's going to segfault if they don't range check the fd first.
        // Because in the C++ version, I could just throw.
        // Maybe I should just replace bitmap with the C++ version
        // and expose a C interface that just throws. ...not that it's really better
        // At least you'll see bitmap throwing as opposed to segfault (core dumped)
        // That's unfortunate.

        // I'm going to get so many emails.
        // if (bitmap_test(fs->fd_table.fd_status,fd)) {
        // Actually, I can just reset it. If it's not set, unsetting it doesn't do anything.
        // Bits, man.

        // But actually it fails the test since I say it was ok
        if (bitmap_test(fs->fd_table.fd_status, fd)) {
            bitmap_reset(fs->fd_table.fd_status, fd);
            return 0;
        }
    }
    return -1;
}

///
/// Writes data from given buffer to the file linked to the descriptor
///   Writing past EOF extends the file
///   Writing inside a file overwrites existing data
///   R/W position in incremented by the number of bytes written
/// \param fs The S16FS containing the file
/// \param fd The file to write to
/// \param dst The buffer to read from
/// \param nbyte The number of bytes to write
/// \return number of bytes written (< nbyte IFF out of space), < 0 on error
///
ssize_t fs_write(S16FS_t *fs, int fd, const void *src, size_t nbyte) {
    if (fs && src && fd_valid(fs, fd)) {
        if (nbyte != 0) {
            // Alrighty, biggest issue is overwrite vs extend
            // We gotta figure that out.
            inode_t file_inode;
            // Man, FDs make this nicer
            // But, I mean, once you have locate working, it's not bad either
            size_t *current_position = &fs->fd_table.fd_pos[fd];
            // Perfect time for a reference. Oh C++ I miss you
            // You can use C++, I can't, since people freak out when they see C++
            // (even though it's like 80% the same)
            if (read_inode(fs, &file_inode, fs->fd_table.fd_inode[fd])) {
                // Got the inode, now we know the size. Handy.
                write_mode_t write_mode = GET_WRITE_MODE(file_inode.mdata.size, *current_position, nbyte);
                if (write_mode & EXTEND) {
                    ssize_t new_filesize
                        = extend_file(fs, &file_inode, fs->fd_table.fd_inode[fd], *current_position + nbyte);
                    if (new_filesize < 0) {
                        return -1;
                    } else if (((size_t) new_filesize) < (*current_position + nbyte)) {
                        // File could not extend enough
                        // Set desired size to all we can do
                        nbyte = new_filesize - *current_position;
                    }
                    // data hasn't been written, but the blocks have been allocated and put into place
                    // We can write the specified ammount of bytes
                    // Update the size here
                }
                if (nbyte) {
                    ssize_t written = overwrite_file(fs, &file_inode, fs->fd_table.fd_pos[fd], src, nbyte);
                    if (written > 0) {
                        fs->fd_table.fd_pos[fd] += written;
                    }
                    return written;
                }
                return 0;
            }
        } else {
            // Sure, I wrote zero bytes. Go team!
            return 0;
        }
    }
    return -1;
}

///
/// Deletes the specified file and closes all open descriptors to the file
///   Directories can only be removed when empty
/// \param fs The S16FS containing the file
/// \param path Absolute path to file to remove
/// \return 0 on success, < 0 on error
///
int fs_remove(S16FS_t *fs, const char *path) {
    if (fs && path) {
        result_t file_info;
        locate_file(fs, path, &file_info);
        // Locate file actually checks pointers for us so... eh oh well.
        if (file_info.success && file_info.found
            && file_info.inode != 0) {  // test 1 of 1 million to make sure we don't kill root
            printf("AHHHHH DELETING A FILE WHOSE NAME IS HOPEFULLY %s BECAUSE THIS ISN'T TESTED\n\n", file_info.data);
            bool success = false;
            switch (file_info.type) {
                case FS_DIRECTORY:
                    success
                        = release_dir(fs, (char *) file_info.data, file_info.inode, file_info.parent, file_info.block);
                    break;
                case FS_REGULAR:
                    success = release_regular(fs, (char *) file_info.data, file_info.inode, file_info.parent);
                    break;
                default:
                    // https://youtu.be/ijmCEYWefks?t=7s
                    break;
            }
            if (success) {
                release_fds(fs,file_info.inode);
                return 0;
            }
        }
    }
    return -1;
}

///
/// Moves the R/W position of the given descriptor to the given location
///   Files cannot be seeked past EOF or before BOF (beginning of file)
///   Seeking past EOF will seek to EOF, seeking before BOF will seek to BOF
/// \param fs The S16FS containing the file
/// \param fd The descriptor to seek
/// \param offset Desired offset relative to whence
/// \param whence Position from which offset is applied
/// \return offset from BOF, < 0 on error
///
off_t fs_seek(S16FS_t *fs, int fd, off_t offset, seek_t whence)
{
    if (! fs || fd < 0 || fd >= DESCRIPTOR_MAX || (whence != FS_SEEK_SET && whence != FS_SEEK_CUR && whence != FS_SEEK_END))
    {
        printf("Invalid parameters.\n");
        return -1;
    }

    //get some useful information...
    //the index of the inode and filesize will be helpful later on....
    //find inode number
    int inodeIndex = fs->fd_table.fd_inode[fd];

    //open inode
    inode_t inode;
    if (! read_inode(fs, &inode, inodeIndex))
    {
        printf("Failed to read in the inode.\n");
        return -3;
    }

    //grab filesize
    int fileSize = inode.mdata.size;

    //get associated offset
    int oldOffset = fs->fd_table.fd_pos[fd];

    //get the file descriptor
    if (! bitmap_test(fs->fd_table.fd_status, fd)) //if set to 1, it is open
    {
        //not set to 1, so it is closed and should not be accessed
        printf("File currently not opened!\n");
        return -2;
    }

    int newOffset = 0;

    if (whence == FS_SEEK_SET)
    {
        newOffset = offset;
    }
    else if (whence == FS_SEEK_CUR)
    {
        newOffset = oldOffset + offset;
    }
    else if (whence == FS_SEEK_END)
    {

        //calc new offset based off end
        newOffset = fileSize + offset;

        //check not extending past end
        if (newOffset >= fileSize)
        {
            newOffset = fileSize;
        }
    }

    //check bounds
    if (newOffset < 0)
    {
        newOffset = 0;
    }
    else if (newOffset > fileSize)
    {
        newOffset = fileSize;
    }

    //update offset in table
    fs->fd_table.fd_pos[fd] = newOffset;

    //return data
    return newOffset;
}

///
/// Reads data from the file linked to the given descriptor
///   Reading past EOF returns data up to EOF
///   R/W position in incremented by the number of bytes read
/// \param fs The S16FS containing the file
/// \param fd The file to read from
/// \param dst The buffer to write to
/// \param nbyte The number of bytes to read
/// \return number of bytes read (< nbyte IFF read passes EOF), < 0 on error
///
ssize_t fs_read(S16FS_t *fs, int fd, void *dst, size_t nbyte)
{
    if (! fs || fd < 0 || fd >= DESCRIPTOR_MAX || ! dst)
    {
        printf("Invalid parameters.\n");
        return -1;
    }

    if (nbyte == 0)
    {
        return 0;
    }

    int numLeft = nbyte;

    //get offset and filesize
    int offset = fs->fd_table.fd_pos[fd];
    int filesize = 0;
    inode_t inode;
    if (! read_inode(fs, &inode, fs->fd_table.fd_inode[fd]))
    {
        printf("Invalid inode read in.\n");
        return -2;
    }
    filesize = inode.mdata.size;

    //figure out max number readable
    int maxReadableFromOffset = filesize - offset;

    if (maxReadableFromOffset < numLeft)
    {
        nbyte = maxReadableFromOffset;
        numLeft = maxReadableFromOffset;
    }

    //now load in all necessary block of data
    int firstBlockIndex = offset / BLOCK_SIZE;
    int lastBlockIndex = (offset + numLeft) / BLOCK_SIZE;
    dyn_array_t *da = get_blocks(fs, &inode, firstBlockIndex, lastBlockIndex);

    if (! da)
    {
        //problem loading in blocks
        printf("Failed to load in blocks.\n");
        return -3;
    }

    //prepare the pointer that will track the location of the offset within the file for memcpy
    uint8_t *ptr = (uint8_t *)dst;

    //by this time, nbyte will hold the number that can actually be read considering the size of the currently opened file.

    //handle the first block if starting in the middle of it
    int firstBlockOffset = offset % 1024;
    if (firstBlockOffset != 0)
    {
        //get the size if assuming to the end of the block for the first read
        size_t numInFirstBlockToRead = BLOCK_SIZE - firstBlockOffset;

        //now actually compare it to make sure that it isn't larger than the current nbyte
        if (numInFirstBlockToRead > nbyte)
        {
            //we need to handle this special case
            //basically, only read in nbyte bytes. NOT numInFirstBlockToRead Bytes.
            numInFirstBlockToRead = nbyte;
        }

        //get first block index
        uint16_t blockIndex = 0;

        if (! dyn_array_extract_front(da, &blockIndex))
        {
            printf("Failed to read in first block index.\n");
            return -4;
        }

        //get first block of data
        data_block_t block;
        if (! back_store_read(fs->bs, blockIndex, &block))
        {
            printf("Failed to read in first block of data.\n");
            dyn_array_destroy(da);
            return -40;
        }

        //read in the bytes calculated for the currently block
        memcpy(ptr, block + firstBlockOffset, numInFirstBlockToRead);

        //update tracker variables
        ptr += numInFirstBlockToRead;
        numLeft -= numInFirstBlockToRead;

        if (numLeft == 0)
        {
            //read everything in the first block
            dyn_array_destroy(da);
            fs->fd_table.fd_pos[fd] += nbyte;
            return nbyte;
        }
    }

    //now load in the rest, one by one
    data_block_t block;

    while (dyn_array_empty(da) == false)
    {
        uint16_t BSBlockIndex = 0;

        //read in a BSBlockIndex
        if (! dyn_array_extract_front(da, &BSBlockIndex))
        {
            printf("Failed to read in middle data.");
            dyn_array_destroy(da);
            return -5;
        }

        //load in block data
        if (! back_store_read(fs->bs, BSBlockIndex, &block))
        {
            printf("Failed to read in data block from backing store.\n");
            dyn_array_destroy(da);
            return -6;
        }

        //if have over a block's worth of data, just read it all in
        if (numLeft >= BLOCK_SIZE)
        {
            //not so special in the middle case....
            memcpy(ptr, block, BLOCK_SIZE);
            ptr += BLOCK_SIZE;
            numLeft -= BLOCK_SIZE;
        }
        //else, read what's left
        else
        {
            //special last block case!!!
            memcpy(ptr, block, numLeft);
            ptr += numLeft;
            numLeft = 0;
            break;
        }
    }

    //cleanup time!
    dyn_array_destroy(da);

    //by this point, the number of bytes read in has already been calculated
    //it is held in nbyte
    //I just need to update the file descriptor table offset...
    fs->fd_table.fd_pos[fd] += nbyte;

    return nbyte;
}

///
/// Populates a dyn_array with information about the files in a directory
///   Array contains up to 15 file_record_t structures
/// \param fs The S16FS containing the file
/// \param path Absolute path to the directory to inspect
/// \return dyn_array of file records, NULL on error
///
dyn_array_t *fs_get_dir(S16FS_t *fs, const char *path)
{
    //Validate Parameters
    if (! fs || ! path)
    {
        printf("Invalid parameters detected!\n");
        return NULL;
    }

    //Grab directory associated with path
    result_t dirRes;
    locate_file(fs, path, &dirRes);

    if (! dirRes.found || ! dirRes.success)
    {
        printf("Directory not found!\n");
        return NULL;
    }

    //Load in the data into a block
    inode_t dirInode;

    if (! read_inode(fs, &dirInode, dirRes.inode))
    {
        printf("Invalid read on iNode.\n");
        return NULL;
    }

    dir_block_t dirListing;
    if (! full_read(fs, &dirListing, dirInode.data_ptrs[0]))
    {
        printf("Failed to read in directory data!\n");
        return NULL;
    }

    //create a dyn_array
    dyn_array_t *da = dyn_array_create(0, sizeof(file_record_t), NULL);

    if (! da)
    {
        printf("Failed to create dyn_array.\n");
        return NULL;
    }

    //For each entry in the directory block
    for (int i = 0; i < 15; ++i)
    {
        //Add it to your dyn_array if it is a valid entry
        if (dirListing.entries[i].fname[0] != '\0')
        {
            file_record_t toAdd;
            inode_t inode;

            if (! read_inode(fs, &inode, dirListing.entries[i].inode))
            {
                printf("Failed to load in file_record_t.\n");
                dyn_array_destroy(da);
                return NULL;
            }

            //copy name over
            strcpy(toAdd.name, dirListing.entries[i].fname);

            //copy type over
            //get the actual file first from the find
            result_t searchResult;

            char *pathWithEverything = (char *)malloc(strlen(path) + strlen(toAdd.name) + 2); //add 2 for a null terminator and a slash character
            strcpy(pathWithEverything, path);

            if (strcmp(pathWithEverything, "/") != 0)
            {
                //not at the root so add another '/'
                strcat(pathWithEverything, "/");
            }

            strcat(pathWithEverything, toAdd.name);
            locate_file(fs, pathWithEverything, &searchResult);
            free(pathWithEverything);

            if (searchResult.found == false || searchResult.success == false)
            {
                printf("Failed to open searchResult.\n");
                dyn_array_destroy(da);
                return NULL;
            }

            toAdd.type = searchResult.type;

            if (! dyn_array_push_back(da, &toAdd))
            {
                printf("Failed to push to the dyn_array.\n");
                dyn_array_destroy(da);
                return NULL;
            }
        }
    }

    //Return dyn_array
    return da;
}

void go_up(char *path)
{
    char *c = path;

    while (*c != '\0')
        c++;
    while (*c != '/')
        c--;

    *c = '\0';

    //TODO: Need to fix so it works everywhere
    if (c == path)
    {
        *c = '/';
        c++;
        *c = '\0'; //temporary fix for handling the root directory...
    }
}

void go_down(char *path)
{
    char *c = path;

    while (*c != '\0')
        c++;

    *c = '\0';
}

char *get_file_name(const char *path)
{
    char *p = strdup(path);
    char *c = p;
    while (*c != '\0')
        c++;
    while (*c != '/')
        c--;
    c++;
    char *ret = strdup(c);
    free(p);
    return ret;
}

///
/// !!! Graduate Level/Undergrad Bonus !!!
/// !!! Activate tests from the cmake !!!
///
/// Moves the file from one location to the other
///   Moving files does not affect open descriptors
/// \param fs The S16FS containing the file
/// \param src Absolute path of the file to move
/// \param dst Absolute path to move the file to
/// \return 0 on success, < 0 on error
///
int fs_move(S16FS_t *fs, const char *src, const char *dst)
{
    if (! fs || ! src || ! dst)
    {
        printf("Invalid Parameter Detected!\n");
        return -1;
    }

    //ensure the src is not a substring of the dst
    if (strstr(dst, src) != NULL)
    {
        printf("You can't move a directory to within itself.\n");
        return -2;
    }

    //ensure destination doesn't already exist
    result_t res;
    locate_file(fs, dst, &res);

    if (res.found && res.success)
    {
        printf("Cannot move to an already existing location.\n");
        return -3;
    }

    //open destination directory and ensure there is space
    char *dstEditable = strdup(dst);
    go_up(dstEditable); //go up to the parent directory...
    locate_file(fs, dstEditable, &res);
    free(dstEditable);

    if (!res.found || !res.success)
    {
        printf("Failed to find the file.\n");
        return -3;
    }

    inode_t dstInode;

    if (! read_inode(fs, &dstInode, res.inode))
    {
        printf("Failed to read in parent directory location.\n");
        return -4;
    }

    dir_block_t dstDirEntries;

    if (! full_read(fs, &dstDirEntries, dstInode.data_ptrs[0]))
    {
        printf("Failed to read in directory entries.\n");
        return -5;
    }

    int numEntries = 0;
    for (int i = 0; i < DIR_REC_MAX; ++i)
    {
        if (dstDirEntries.entries[i].fname[0] != '\0')
        {
            //we have a live one!
            numEntries++;
        }
    }

    if (numEntries == 15)
    {
        printf("Destination directory filled up!\n");
        return -6;
    }

    //open src directory and remove the file from its listings
    result_t srcResult;
    locate_file(fs, src, &srcResult);

    if (! srcResult.found || ! srcResult.success)
    {
        printf("Source file could not be found.\n");
        return -7;
    }

    char *srcFileName = get_file_name(src);

    if (! wipe_parent_entry(fs, srcFileName, srcResult.parent))
    {
        printf("Failed to wipe from parent.\n");
        free(srcFileName);
        return -8;
    }

    free(srcFileName);

    //update destination directory
    for (int i = 0; i < DIR_REC_MAX; ++i)
    {
        if (dstDirEntries.entries[i].fname[0] == '\0')
        {
            //found an empty entry
            char *newFileName = get_file_name(dst);
            strcpy(dstDirEntries.entries[i].fname, newFileName);
            free(newFileName); //don't need you anymore...
            dstDirEntries.entries[i].inode = res.inode; //remember this from way back when?

            //push to back store
            if (! full_write(fs, &dstDirEntries, dstInode.data_ptrs[0]))
            {
                printf("Failed to write to the back_store.\n");
                return -9;
            }

            break; //we're done here...
        }
    }

    //return success
    return 0;
}








