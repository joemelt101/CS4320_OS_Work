#include "S16FS.h"
#include <sys/types.h>
#include <dyn_array.h>
#include <back_store.h>
#include <string.h>

typedef struct
{
    int length;
    int num_parts;
    char* start;
} path_t;

/**
    PURPOSE:
        Creates a path object that makes it convenient to search through a path name
    PARAMETERS:
        path: The path to create a path_t object out of
    RETURNS:
        A path_t object or NULL if invalid path or failed operation
**/
static path_t* path_create(const char* path)
{
    if (! path || path[0] != '/')
    {
        return NULL;
    }

    //TODO: return NULL if an invalid path is detected
    path_t *ret = (path_t *) malloc(sizeof(path_t) - 1); //ignore the first slash
    ret->num_parts = 1;
    ret->length = strlen(path); //add 1 to include null terminator in length (-1 for beginning slash and +1 for null terminator = +0)

    //copy over path data
    char* c = (char *)malloc(sizeof(char) * ret->length);
    int pLen = 0;

    if (! c)
    {
        free(ret);
        return NULL;
    }

    //link up path data
    ret->start = c;

    memcpy(c, (path + 1), ret->length); //add one to ignore the first slash

    while (*c != '\0')
    {
        if (*c == '/')
        {
            //swap out
            *c = '\0';
            ret->num_parts++;
        }

        ++c; //increment
        pLen++;

        if (pLen > FS_FNAME_MAX)
        {
            //too long for current part
            path_destroy(ret);
            return NULL;
        }
    }

    return ret;
}

/**
    PURPOSE:
        Gets the zero based index part of the path
    PARAMETERS:
        path: the path_t object to get the part of
        part: the zero based idnex part of the path to get
    RETURNS:
        The full string of the part of the directory to get
**/
static char* path_get_part(path_t *path, int part)
{
    if (! path)
    {
        return NULL;
    }

    if (part == 0)
    {
        return path->start;
    }

    for (int i = 0; i < path->length; ++i)
    {
        if (path->start[i] == '\0')
        {
            part--;
        }

        if (part == 0)
        {
            return &path->start[i + 1];
        }
    }

    return NULL;
}

/**
    PURPOSE:
        Returns the string data passed to the path to its original format and frees memory
    PARAMETERS:
        path: The path object to free
    RETURNS:
        Nothing
**/
static void path_destroy(path_t* path)
{
    free(path->start);
    free(path);
}

/**
    PURPOSE:
        Traverses the file system to return the associated file record index.
    PARAMETERS:
        path: The path to find
        fs: The filesystem to traverse through
    RETURNS:
        A number > 0 if success
        0 on failure
**/
static uint8_t fs_traverse(S16FS_t *fs, const char *path)
{
    if (! path)
    {
        //invalid path
        return NULL;
    }

    path_t *p = path_create(path);

    if (! p)
    {
        //path is invalid
        return 0;
    }

    //load root directory
    file_record_t *f = &fs->file_records[0];

    if (f.type != FS_DIRECTORY)
    {
        //not a directory and it's the root!
        //signal error
        path_destroy(p);
        return 0;
    }

    //allocate block
    uint8_t block[1024];

    //return index
    uint8_t ret = 0;

    for (int i = 0; i < p->num_parts; ++i)
    {
        ////////////////////////////
        //scan dir for next filename

        //first load the dir data
        if (back_store_read(fs->bs, f->block_refs[0], block) == false)
        {
            path_destroy(p);
            return 0;
        }

        //scan loaded dir data for next part of path
        char* next = path_get_part(p, i);

        directory_t *dir = block;

        for (int j = 0; j < 15; ++j)
        {
            if (strcmp(dir->entries[j].file_name, next) == 0)
            {
                //found the next part!
                f = fs->file_records[dir->entries[j].file_record_index];
                ret = dir->entries[j].file_record_index; //record location of next file
            }

            if (j == 14)
            {
                //next part not in directory...
                path_destroy(p);
                return 0;
            }
        }

        if (i == p->num_parts - 1)
        {
            //reg or dir filetype and at the end of path, so we found our file
            break;
        }
        else if (f.type == FS_REGULAR && i < p->num_parts - 1)
        {
            //reg filetype and there is still some distance to go
            //error!
            ret 0;
            break;
        }
        else if (f.type == FS_DIRECTORY && i < p->num_parts - 1)
        {
            //dir filetype and there is some distance to go
            //load next directory and continue
            continue;
        }
        else
        {
            //some uncaught error
            ret = 0;
            break;
        }
    }

    path_destroy(p);
    return ret;
}

// Milestone 1

///
/// Formats (and mounts) an S16FS file for use
/// \param fname The file to format
/// \return Mounted S16FS object, NULL on error
///
S16FS_t *fs_format(const char *path)
{
    if (! path)
    {
        return NULL;
    }

    /*
    printf("File Record Size: %d\n", sizeof(file_record_t));
    printf("Directory Struct Size: %d\n", sizeof(directory_t));
    */


    char path2[50] = "/hello/world/how/are/you.c";
    path_t *p = create_path(path2);

    for (int i = 0; i < 6; ++i)
    {
        printf("%d: %s\n", i, get_part(p, i));
    }

    printf("%s\n", path2);

    destroy_path(p);

    printf("%s\n", path2);


    //create, flush, and close a back_store to setup the foundation for the S16FS file
    back_store *bs = back_store_create(path);

    if (bs == NULL)
    {
        //an error occurred
        return NULL;
    }

    uint8_t data[1024] = {0}; //create a KB of zeroed out data

    //allocate and flush proper data to the file
    for (int i = 8; i < 40; ++i)
    {
        if (! back_store_request(bs, i))
        {
            //something failed to allocate
            back_store_close(bs);
            return NULL;
        }

        if (! back_store_write(bs, i, data))
        {
            //cleanup and return
            back_store_close(bs);
            return NULL;
        }
    }

    //mount the image
    S16FS_t *fs = (S16FS_t *) calloc(1, sizeof(S16FS_t));

    if (! fs)
    {
        return NULL;
    }

    fs->bs = bs;

    //return newly created object
    return fs;
}

///
/// Mounts an S16FS object and prepares it for use
/// \param fname The file to mount
/// \return Mounted F16FS object, NULL on error
///
S16FS_t *fs_mount(const char *path)
{
    if (! path)
    {
        return NULL;
    }

    back_store_t *bs = back_store_open(path);

    if (! bs)
    {
        //failed to open backing store...
        return NULL;
    }

    S16FS_t *fs = (S16FS_t *) calloc(1, sizeof(S16FS_t));

    if (! fs)
    {
        back_store_close(bs);
        return NULL;
    }

    //Set BS
    fs->bs = bs;

    //8 files in 1 block of data
    file_record_t block[8];

    //load file record data from bs
    for (int i = 8; i < 40; ++i)
    {
        //8 inodes per block of data
        //load block associated with i
        if (back_store_read(fs->bs, i, block) == false)
        {
            //error occurred reading in data
            free(fs);
            back_store_close(bs);
            return NULL;
        }

        //transfer block data to file system file_records array, 8 at a time
        memcpy(&fs->file_records[8*(i-8)], block, sizeof(block));
    }

    return fs;
}

///
/// Unmounts the given object and frees all related resources
/// \param fs The S16FS object to unmount
/// \return 0 on success, < 0 on failure
///
int fs_unmount(S16FS_t *fs)
{
    if (! fs)
    {
        return -1;
    }

    //flush inode table to bs
    for (int i = 8; i < 40; ++i)
    {
        if (back_store_write(fs->bs, i, &fs->file_records[8*i]) == false)
        {
            back_store_close(fs->bs);
            free(fs);
            return -2;
        }
    }

    //close the backing store
    back_store_close(fs->bs);

    //free the S16FS_t object
    free(fs);

    return 0;
}

///
/// Creates a new file at the specified location
///   Directories along the path that do not exist are not created
/// \param fs The S16FS containing the file
/// \param path Absolute path to file to create
/// \param type Type of file to create (regular/directory)
/// \return 0 on success, < 0 on failure
///
int fs_create(S16FS_t *fs, const char *path, file_t type)
{
    if (! fs || ! path)
    {
        return NULL;
    }

    if (type != FS_REGULAR && type != FS_DIRECTORY)
    {
        return NULL;
    }

    //traverse to containing directory
    fs_traverse(fs, path);

    return 0;
}

// Milestone 2

///
/// Opens the specified file for use
///   R/W position is set to the beginning of the file (BOF)
///   Directories cannot be opened
/// \param fs The S16FS containing the file
/// \param path path to the requested file
/// \return file descriptor to the requested file, < 0 on error
///
int fs_open(S16FS_t *fs, const char *path)
{
    if (! fs || ! path)
    {
        return NULL;
    }

    return -1;
}

///
/// Closes the given file descriptor
/// \param fs The S16FS containing the file
/// \param fd The file to close
/// \return 0 on success, < 0 on failure
///
int fs_close(S16FS_t *fs, int fd)
{
    if (! fs || fd > _MAX_NUM_OPEN_FILES || fd < 0)
    {
        return -1;
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
ssize_t fs_write(S16FS_t *fs, int fd, const void *src, size_t nbyte)
{
    if (! fs || fd < 0 || fd > _MAX_NUM_OPEN_FILES || ! src || nbyte == 0)
    {
        return -1;
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
int fs_remove(S16FS_t *fs, const char *path)
{
    if (! fs || ! path)
    {
        return -1;
    }

    return -1;
}

// Milestone 3

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
    if (! fs || fd < 0 || fd > _MAX_NUM_OPEN_FILES)
    {
        return -1;
    }

    //TODO: Check offset and whence
    if (offset || whence)
    {
        return -1;
    }

    return -1;
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
    if (! fs || fd < 0 || fd > _MAX_NUM_OPEN_FILES || ! dst || nbyte == 0)
    {
        return -1;
    }

    return -1;
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
    if (! fs || ! path)
    {
        return NULL;
    }

    return NULL;
}


// Extra Credit!!! :D

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
        return NULL;
    }

    return -1;
}
