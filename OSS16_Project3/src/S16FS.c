#include "S16FS.h"
#include <sys/types.h>
#include <dyn_array.h>
#include <back_store.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct
{
    int length;
    int num_parts;
    char* start;
} path_t;

//Prototypes
static void path_destroy(path_t *);

///////////////////////
// Helper Functions ///
///////////////////////

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
    //validate that the path exists and it starts with '/'
    if (! path || path[0] != '/')
    {
        return NULL;
    }

    //validate it doesn't end with a slash
    if (path[strlen(path) - 1] == '/')
    {
        return NULL;
    }

    //check for just '/'
    if (strlen(path) == 1)
    {
        //also a problem
        return NULL;
    }

    path_t *ret = (path_t *) malloc(sizeof(path_t)); //ignore the first slash
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
    Purpose:
        Returns the number of parts in a given path object.
    Parameters:
        path: The path to return the number of parts of.
    Returns:
        The number of parts in the provided path. Returns -1 if a null object is passed.

static int path_get_num_parts(path_t *path)
{
    if (! path)
    {
        return -1;
    }

    return path->num_parts;
} **/

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

    if (part >= path->num_parts)
    {
        return NULL;
    }

    for (int i = 0; i < path->length; ++i)
    {
        if (path->start[i] == '\0')
        {
            part--;
        }

        if (part == 0)
        {
            char* res = &path->start[i + 1];

            if (*res == '\0')
            {
                return NULL;
            }

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
    if (! path)
    {
        return;
    }

    free(path->start);
    free(path);
}

/**
    PURPOSE:
        Returns a block id for a given file offset, allocates if necessary
    PARAMETERS:
        fs: A pointer to the file system object
        file: A pointer to the file record
        fileBlockIndex: The file (block) offset to use to retrieve a block
    RETURNS:
        Success: An integer containing the backing store block id associated with the provided offset and file record
        Failure: A negative integer
**/
static int get_block_bs_index(S16FS_t *fs, file_record_t *fileRecord, uint32_t fileBlockIndex)
{
    if (! fileRecord)
    {
        //invalid parameters
        return -1;
    }

    ///////////////////////////////////////
    //Traverse to the block within the file

    if (fileBlockIndex < 6) //if within index of 6
    {
        //if need to allocate, then do so
        if (fileRecord->block_refs[fileBlockIndex] == 0)
        {
            int res = back_store_allocate(fs->bs);

            if (res == 0)
            {
                //printf("Failed to allocate file block.\n");
                return -10;
            }

            fileRecord->block_refs[fileBlockIndex] = res;
        }

        //in a direct block, easy to return
        return fileRecord->block_refs[fileBlockIndex];
    }
    else if (fileBlockIndex <= 517) //if between [6, 517]
    {
        //in an indirect block

        //if indirect block doesn't exist, add it
        if (fileRecord->block_refs[6] == 0)
        {
            int res = back_store_allocate(fs->bs);

            if (res == 0)
            {
                //failed to allocate location
                //printf("Failed to allocate file block.\n");
                return -1;
            }

            //file block successfully allocated
            fileRecord->block_refs[6] = res;
        }

        //calculate which indirect block is needed
        int directBlockIndex = fileBlockIndex - 6; //subtract 6 to remove direct blocks passed over

        //grab the indirect block of data
        uint8_t data[1024];
        int indirectBlockBSIndex = fileRecord->block_refs[6];

        if (! back_store_read(fs->bs, indirectBlockBSIndex, &data))
        {
            //error reading from backing store
            //printf("Failed to read from backing_store in get_block_bs_index.\n");
            return -2;
        }

        //cast data block to readable format
        uint16_t *directPointers = (uint16_t *)data; //512 of these...

        if (directPointers[directBlockIndex] == 0)
        {
            //need to allocate block in ID block and push to backing store.
            int res = back_store_allocate(fs->bs);

            if (res == 0)
            {
                //printf("Failed to allocate.\n");
                return -3;
            }

            directPointers[directBlockIndex] = res;

            if (! back_store_write(fs->bs, res, directPointers)) //update direct pointers
            {
                //printf("Failed to write direct pointers back to back_store.\n");
                return -4;
            }
        }

        //return the address
        return directPointers[directBlockIndex];
    }
    else if (fileBlockIndex <= 262661) // should be [518->262,661]
    {
        if (fileRecord->block_refs[7] == 0)
        {
            //need to allocate DID block
            int res = back_store_allocate(fs->bs);

            if (res == 0)
            {
                //printf("Failed to allocated DID block.\n");
                return -5;
            }

            fileRecord->block_refs[7] = res;
        }

        //in a double indirect block
        int indirectBlockIndex = (fileBlockIndex - 518) / 512; //subtract 518 to remove direct and indirect block locations and divide by 516 to ensure proper one received
        int directBlockIndex = (fileBlockIndex - 518) % 512;

        //load in indirect pointers
        uint8_t data[1024];

        int doubleIndirectBlockBSIndex = fileRecord->block_refs[7];

        if (! back_store_read(fs->bs, doubleIndirectBlockBSIndex, &data))
        {
            //failed to read data from BS
            //printf("Failed to read DID from BS.\n");
            return -3;
        }

        uint16_t *indirectPointers = (uint16_t *)data;

        if (indirectPointers[indirectBlockIndex] == 0)
        {
            //need to allocate a block here
            int res = back_store_allocate(fs->bs);

            if (res == 0)
            {
                //printf("Failed to allocate ID inside of DID.\n");
                return -6;
            }

            indirectPointers[indirectBlockIndex] = res;

            if (! back_store_write(fs->bs, fileRecord->block_refs[7], indirectPointers))
            {
                //printf("Failed to write ID inside of DID.\n");
                return -7;
            }
        }

        int readFrom = indirectPointers[indirectBlockIndex];

        //now find indirect block
        if (! back_store_read(fs->bs, indirectPointers[indirectBlockIndex], &data))
        {
            //failed to read from BS
            //printf("Failed to read from backing store.\n");
            return -4;
        }

        uint16_t *directPointers = (uint16_t *)data;

        if (directPointers[directBlockIndex] == 0)
        {
            //need to allocate a block here
            int res = back_store_allocate(fs->bs);

            if (res == 0)
            {
                //printf("Failed to create D in DI in DID.\n");
                return -8;
            }

            directPointers[directBlockIndex] = res;

            if (! back_store_write(fs->bs, readFrom, directPointers))
            {
                //printf("Attempted location: %d\n", indirectPointers[indirectBlockIndex]);
                //printf("Failed to write D in DI in DID.\n");
                return -9;
            }
        }

        //now find direct location
        return directPointers[directBlockIndex];
    }

    return -5; //out of bounds
}

/**
    PURPOSE:
        Finds the first available location in the provided file system object (fs) and formats a file record there.
    PARAMETERS:
        fs: The file system format a new file record in
        filename: The name of the new file to format
        filetype: The type of the file to create
    RETURNS:
        NULL if error
        The pointer to the file if everything worked out
**/
static file_record_t* allocate_file_record(S16FS_t* fs, char* filename, file_t filetype)
{
    if (! fs || ! filename || (filetype != FS_REGULAR && filetype != FS_DIRECTORY))
    {
        printf("Invalid parameters passed to create_file_record\n.");
        return NULL;
    }

    //discover open location to place file
    for (int i = 0; i < _MAX_NUM_FS_FILES; ++i)
    {
        if (fs->file_records[i].name[0] == '\0')
        {
            //found an index of an open location!
            //grab file
            file_record_t* f = &fs->file_records[i];

            //now set its properties

            //set the name
            memcpy(f->name, filename, strlen(filename) + 1); //add one to include the null terminator

            //set the type
            f->type = filetype;

            //clear this out
            f->metadata.fileSize = 0;
            f->metadata.virtualFileSize = 0;

            //clear out block_refs
            for (int j = 0; j < 8; ++j)
            {
                f->block_refs[j] = 0;
            }

            //if a directory, allocate first block to point to first open location
            if (f->type == FS_DIRECTORY)
            {
                uint32_t index = back_store_allocate(fs->bs);

                if (index == 0)
                {
                    printf("Failed to allocate block from the back store in allocate_file_record.\n");
                    return NULL;
                }

                //else, a valid location was allocated
                f->block_refs[0] = index;

                //clear block at index
                uint8_t block[1024] = {0};

                if (! back_store_write(fs->bs, index, block))
                {
                    printf("Failed to write to the backing store in allocate_file_record!\n");
                    return NULL;
                }

                f->metadata.fileSize = 1024; //set this for deletion purposes
                f->metadata.virtualFileSize = 1024;
            }

            //return the newly allocated location
            return f;
        }
    }

    //made it through without a single open spot...
    printf("Failed in create_file_record: The maximum number of files has already been allocated!\n");
    return NULL;
}

/**
    PURPOSE:
        Traverses the file system to return the associated file record index.
    PARAMETERS:
        path: The path to find (ex: "a/b/c") (ex: "" = root = 0)
        fs: The filesystem to traverse through
    RETURNS:
        A number >= 0 if success
        -1 on failure
**/
static int fs_traverse(S16FS_t *fs, const char *path)
{
    if (! path)
    {
        //invalid path
        return -1;
    }

    if (strlen(path) == 0)
    {
        return 0;
    }

    path_t *p = path_create(path);

    if (! p)
    {
        //path is invalid
        return -1;
    }

    //load root directory
    file_record_t *f = &fs->file_records[0];

    if (f->type != FS_DIRECTORY)
    {
        //not a directory and it's the root!
        //signal error
        path_destroy(p);
        return -1;
    }

    //allocate block
    uint8_t block[1024];

    //return index
    int ret = 0;

    for (int i = 0; i < p->num_parts; ++i)
    {
        ////////////////////////////
        //scan dir for next filename

        //first load the dir data
        if (back_store_read(fs->bs, f->block_refs[0], block) == false)
        {
            path_destroy(p);
            return -1;
        }

        //scan loaded dir data for next part of path
        char* next = path_get_part(p, i);

        directory_t *dir = (directory_t *)block;

        for (int j = 0; j < 15; ++j)
        {
            if (strcmp(dir->entries[j].file_name, next) == 0)
            {
                //found the next part!
                f = &fs->file_records[dir->entries[j].file_record_index];
                ret = dir->entries[j].file_record_index; //record location of next file
                break;
            }

            if (j == 14)
            {
                //next part not in directory...
                path_destroy(p);
                return -1;
            }
        }

        if (i == p->num_parts - 1)
        {
            //reg or dir filetype and at the end of path, so we found our file
            break;
        }
        else if (f->type == FS_REGULAR && i < p->num_parts - 1)
        {
            //reg filetype and there is still some distance to go
            //error!
            ret = -1;
            break;
        }
        else if (f->type == FS_DIRECTORY && i < p->num_parts - 1)
        {
            //dir filetype and there is some distance to go
            //load next directory and continue
            continue;
        }
        else
        {
            //some uncaught error
            ret = -1;
            break;
        }
    }

    path_destroy(p);
    return ret;
}

//////////////////
// Milestone 1 ///
//////////////////

///
/// Formats (and mounts) an S16FS file for use
/// \param fname The file to format
/// \return Mounted S16FS object, NULL on error
///
S16FS_t* fs_format(const char *path)
{
    if (! path)
    {
        printf("Null path passed to fs_format.\n");
        return NULL;
    }

    //////////////////////////
    //Create the backing store

    back_store_t *bs = back_store_create(path);

    if (bs == NULL)
    {
        printf("Failed to create a backing store in fs_format().\n");
        printf("BS creation path: %s.\n", path);
        return NULL;
    }

    ////////////////////////////////////////////////////
    //Create zeroed out inode table in the backing store

    //this will exist in BS blocks 8 through 39 (32 KB total)

    uint8_t data[1024] = {0}; //create a KB of zeroed out data

    for (int i = 8; i < 40; ++i)
    {
        if (! back_store_request(bs, i))
        {
            //something failed to allocate
            printf("Failed to allocate block index %d in the backing store.\n", i);
            back_store_close(bs);
            return NULL;
        }

        if (! back_store_write(bs, i, data))
        {
            printf("Failed to write to block index %d in the backing store.\n", i);
            back_store_close(bs);
            return NULL;
        }
    }

    ///////////////////////////
    //Create File System object

    S16FS_t *fs = (S16FS_t *)calloc(1, sizeof(S16FS_t));

    if (! fs)
    {
        printf("Failed to allocate the file system in main memory.\n");
        back_store_close(bs);
        return NULL;
    }

    fs->bs = bs;

    ////////////////////////////////////////////////////////////////
    //Create and write the first directory file to the backing store

    //create it...
    //do not free root_inode as it will be freed by other means
    char root_name[5] = "root";
    file_record_t *root_inode = allocate_file_record(fs, root_name, FS_DIRECTORY);

    if (! root_inode)
    {
        printf("Error allocating root directory!\n");
        back_store_close(bs);
        return NULL;
    }

    //write it...
    //grab the first block of data (because it's the root directory)
    if (! back_store_read(bs, 8, data))
    {
        printf("Failed to read in first block from the backing store!\n");
        back_store_close(bs);
        return NULL;
    }

    file_record_t *file_block = (file_record_t *)data;
    file_block[0] = *root_inode;

    if (! back_store_write(bs, 8, file_block))
    {
        printf("Failed to write root directory to backing store!\n");
        back_store_close(bs);
        return NULL;
    }

    //Free dummy fs and bs
    free(fs);
    back_store_close(bs);

    /////////////////////////////////////////////
    //Mount the newly created image and return it

    fs = fs_mount(path);

    if (! fs)
    {
        printf("Failed to mount the newly formatted image!\n");
        back_store_close(bs);
        return NULL;
    }

    //return newly created object since all is well...
    return fs;
}

///
/// Mounts an S16FS object and prepares it for use
/// \param fname The file to mount
/// \return Mounted F16FS object, NULL on error
///
S16FS_t *fs_mount(const char *path)
{
    /////////////////////
    //Validate parameters

    if (! path)
    {
        printf("Invalid path passed in!\n");
        return NULL;
    }

    ////////////////////
    //Open backing store

    back_store_t *bs = back_store_open(path);

    if (! bs)
    {
        printf("Failed to open backing store!\n");
        return NULL;
    }

    ///////////////////////////
    //Create file system object

    S16FS_t *fs = (S16FS_t *) calloc(1, sizeof(S16FS_t));

    if (! fs)
    {
        printf("fs_mount: Failed to allocate memory for the file system object!\n");
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
            printf("fs_mount: Failed to read from the backing store!\n");
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
    /////////////////////
    //Validate parameters

    if (! fs)
    {
        printf("Null fs object passed to fs_unmount!\n");
        return -1;
    }

    ////////////////////////////////////////
    //Flush inode table to the backing store

    for (int i = 8; i < 40; ++i)
    {
        if (back_store_write(fs->bs, i, &fs->file_records[(i-8)*8]) == false)
        {
            printf("fs_unmount: Failed to write block to back_store.\n");
            back_store_close(fs->bs);
            free(fs);
            return -2;
        }
    }

    ////////////////
    //Cleanup memory

    back_store_close(fs->bs);
    free(fs);

    ////////////////
    //Return Success

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
    /////////////////////
    //Validate parameters

    if (! fs || ! path)
    {
        printf("fs_create: Bad parameters passed in.\n");
        return -1;
    }

    if (type != FS_REGULAR && type != FS_DIRECTORY)
    {
        printf("fs_create: Bad type plugged in.\n");
        return -1;
    }

    ////////////////
    //Parse the path

    char *path_editable = strdup(path);
    char *path_without_end = NULL;
    char *new_filename = NULL;

    path_t *p = path_create(path_editable);

    if (! p)
    {
        //invalid path detected
        printf("fs_create: Invalid path detected\n");
        free (path_editable);
        return -1;
    }

    path_destroy(p); //no need for it past this

    char *c = path_editable;

    while (*c != '\0')
    {
        c++;
    }
    while (*c != '/')
    {
        c--;
    }

    *c = '\0';

    path_without_end = strdup(path_editable);
    new_filename = strdup(c + 1);

    *c = '/'; //restore editable path

    //////////////////////////////////
    //Traverse to containing directory

    int index = fs_traverse(fs, path_without_end);
    free(path_editable);
    free(path_without_end);

    if (index == -1)
    {
        //couldn't find path specified
        printf("Couldn't find path specified\n");
        free(new_filename);
        return -1;
    }

    ////////////////////////////////////////
    //Create file and add entry to directory

    //get file associated with directory
    file_record_t *rec = &fs->file_records[index];

    //get block of directory entries
    directory_t dir;

    if (back_store_read(fs->bs, rec->block_refs[0], &dir) == false)
    {
        printf("Failed to read from back store!\n");
        free(new_filename);
        return -1;
    }

    //search through for empty spot
    int i = 0;

    for ( ; i < 15; ++i)
    {
        if (strcmp(dir.entries[i].file_name, new_filename) == 0)
        {
            //file already exists in directory -> error!
            printf("File already exists with that name!\n");
            free(new_filename);
            return -1;
        }

        if (dir.entries[i].file_name[0] == '\0')
        {
            //found one!
            break;
        }
    }

    if (i == 15)
    {
        //didn't find an empty spot
        //error!
        printf("Directory is full!\n");
        free(new_filename);
        return -1;
    }

    //else, found an empty spot
    //so populate it
    memcpy(dir.entries[i].file_name, new_filename, sizeof(char) * (strlen(new_filename) + 1));

    //second, create the file object
    int new_file_index = 0;
    file_record_t *new_file = NULL;

    //look for an open spot and set file information
    for (int j = 1; j < _MAX_NUM_FS_FILES; ++j)
    {
        if (fs->file_records[j].name[0] == '\0')
        {
            //found an empty spot! Fill in file data...
            new_file = &fs->file_records[j];
            new_file_index = j;

            memcpy(new_file->name, new_filename, strlen(new_filename) + 1);
            free(new_filename);

            new_file->type = type;

            FileMeta_t *meta = (FileMeta_t *)calloc(1, sizeof(FileMeta_t));

            if (! meta)
            {
                printf("Failed to allocated metadata memory\n");
                return -1;
            }

            new_file->metadata = *meta;
            free(meta);

            for (int z = 0; z < 8; ++z)
                new_file->block_refs[z] = 0;

            if (new_file->type == FS_DIRECTORY)
            {
                //need to add block of data
                int ref = back_store_allocate(fs->bs);

                if (ref == 0)
                {
                    printf("Couldn't allocate space for new directory!\n");
                    return -1;
                }

                uint8_t block[1024] = {0};

                if (back_store_write(fs->bs, ref, block) == false)
                {
                    printf("Failed to zero out space for new directory\n");
                    return -1;
                }

                new_file->block_refs[0] = ref;
            }

            break;
        }
    }

    if (new_file == NULL)
    {
        printf("Couldn't find an open file\n");
        free(new_filename);
        return -1;
    }

    //now, copy the record index of the newly created file
    dir.entries[i].file_record_index = new_file_index;

    //push updated directory data to block
    if (back_store_write(fs->bs, rec->block_refs[0], &dir) == false)
    {
        printf("Failed to write to backing store\n");
        return -1;
    }

    ////////////////
    //Return success
    return 0;
}

//////////////////
// Milestone 2 ///
//////////////////

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
        //printf("fs_open error: null fs or path passed....\n");
        return -1;
    }

    ////////////////////////
    //Get the requested file

    int fileIndex = fs_traverse(fs, path);

    //ensure path exists
    if (fileIndex == -1)
    {
        //error finding the file / directory -- must not exist or be an invalid parameter
        //printf("Couldn't find file: %s.\n", path);
        return -2;
    }

    //Make sure it isn't a directory
    file_record_t* fileRecord = &fs->file_records[fileIndex];

    if (fileRecord->type != FS_REGULAR)
    {
        //Entered path is not a regular file type. May be a directory or an invalid type.
        //printf("%s cannot be opened because it is a directory\n.", path);
        return -3;
    }

    //////////////////////////////
    //Create "new" file descriptor

    //find an open file descriptor
    int fileDescriptorIndex = -1;

    //start at one as to not override the root
    for (uint16_t i = 0; i < _MAX_NUM_OPEN_FILES; ++i)
    {
        //if pointing to the root directory, it is an open location
        if (fs->file_descriptors[i].file_record_index == 0)
        {
            //root used as a way of showing open file descriptors
            //0 means it's open
            //place information inside
            FileDes_t *fd = &fs->file_descriptors[i];
            fd->offset = 0;
            fd->file_record_index = fileIndex;
            fileDescriptorIndex = i;
            break;
        }
    }

    if (fileDescriptorIndex == -1)
    {
        //couldn't find an open descriptor
        //printf("Error: Could not find an open file descriptor.\n");
        return -4;
    }

    /////////////////////////////////////
    //Return the file descriptor location
    //printf("fs_open: %s opened at %d.\n", path, fileDescriptorIndex);
    return fileDescriptorIndex;
}

///
/// Closes the given file descriptor
/// \param fs The S16FS containing the file
/// \param fd The file to close
/// \return 0 on success, < 0 on failure
///
int fs_close(S16FS_t *fs, int fd)
{
    // printf("fs_close called. ID: %d \n", fd);

    if (! fs || fd > _MAX_NUM_OPEN_FILES || fd < 0)
    {
        //invalid parameters
        if (! fs)
        {
            //printf("fs is null and ");
        }
        //printf("Error: Invalid Parameters: fd = %d.\n", fd);
        return -1;
    }

    //////////////////////////////////////////
    //Find the file descriptor and zero it out

    if (fs->file_descriptors[fd].file_record_index == 0)
    {
        //already closed
        //printf("Error: File descriptor already closed.\n");
        return -2;
    }

    //clear out
    fs->file_descriptors[fd].file_record_index = 0;
    fs->file_descriptors[fd].offset = 0;

    ////////////////
    //Return success

    // printf("fs_close succeeded.\n");
    return 0;
}

/**
    PURPOSE: Runs through a list of bs indexes, deallocating them
    PARAMETERS:
        bs: The back_store_t to deallocate from
        arr: The array of indexes
        size: The number of indexes in the array
    RETURNS:
        Nothing
**/
/*
static void deallocate_bs_index_array(back_store_t *bs, int *arr, int size)
{
    for (int i = 0; i < size; ++i)
    {
        back_store_release(bs, arr[i]);
    }
} */

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
    //write to end of current block
    int totalNumberOfBytesOriginally = nbyte;
    uint8_t *ptr = (uint8_t *)src;

    /////////////////////
    //Validate Parameters

    if (nbyte == 0)
    {
        return 0;
    }

    if (! fs || fd < 0 || fd >= _MAX_NUM_OPEN_FILES || ! src)
    {
        //printf("Invalid parameters passed to fs_write.\n");
        return -1;
    }

    printf("fs_write called with fs: %p, fd: %d, src: %p, nbyte: %d.\n", fs, fd, src, nbyte);

    ////////////////////////////
    //Write To End of File First

    //get the file from file descriptors
    FileDes_t *d = &fs->file_descriptors[fd];

    if (d->file_record_index == 0)
    {
        //printf("File not currently opened!\n");
        return -2;
    }

    //printf("Starting offset: %d.\n", d->offset);

    file_record_t *record = &fs->file_records[d->file_record_index];

    if (record->name[0] == '\0')
    {
        //printf("Invalid file record detected.\n");
        return -3;
    }

    //determine whether necessary to write data to the remainder of the current block where the offset is at
    if (d->offset % 1024 != 0)
    {
        //printf("Special case: first block incomplete.\n");
        //not at the end of the last written block, so finish block out
        int lastWrittenBlockIndex = get_block_bs_index(fs, record, d->offset / 1024);
        //printf("Last Written BLock Index: %d\n", lastWrittenBlockIndex);

        int startIndexWithinBlock = d->offset % 1024;
        //printf("Starting index within block: %d\n", startIndexWithinBlock);

        //grab its final block
        uint8_t block[1024] = {0};

        if (! back_store_read(fs->bs, lastWrittenBlockIndex, block))
        {
            //printf("Failed to read from back store.\n");
            return -4;
        }

        //write to that block's end
        int numBytesToWrite = 1024 - startIndexWithinBlock;
        memcpy(block + startIndexWithinBlock, ptr, numBytesToWrite);

        ptr += numBytesToWrite; //keep it moving forward

        //push block back to backing_store
        if (! back_store_write(fs->bs, lastWrittenBlockIndex, block))
        {
            //printf("Failed to write to back store.\n");
            return -5;
        }

        //update num bytes left
        nbyte -= numBytesToWrite;
        d->offset += numBytesToWrite;

        //printf("Special case handled: nbyte = %d and d->offset = %d.\n", nbyte, d->offset);
    }

    //now write blocks one by one to file
    int fileBlockIndex = ceil(d->offset / 1024.0);

    //printf("First file block index: %d.\n", fileBlockIndex);

    while (nbyte > 0)
    {
        int nextID = get_block_bs_index(fs, record, fileBlockIndex++);

        if (nextID < 0)
        {
            //couldn't allocate any more
            //printf("Failed to allocate another block.\n");
            //printf("Bytes written: %d\n", totalNumberOfBytesOriginally);

            int bytesWritten = totalNumberOfBytesOriginally - nbyte;
            record->metadata.fileSize = ceil((float)d->offset / 1024.0) * 1024; //get the number of blocks and multiply by 1024 bytes
            record->metadata.virtualFileSize = record->metadata.fileSize; //ending at the end of a block, so just set to the same thing
            return bytesWritten;
        }

        if (nbyte >= 1024)
        {
            //printf("More than 1024 bytes.\n");
            //printf("nextID: %d\n", nextID);
            //printf("nbytes to write this time: %d.\n", nbyte);

            //write an entire block
            if (! back_store_write(fs->bs, nextID, ptr))
            {
                //printf("Write to the back_store failed.\n");
                return -1;
            }

            ptr += 1024;
            d->offset += 1024;
            nbyte -= 1024;
        }
        else
        {
            //write what's left
            uint8_t block[1024] = {0};
            //printf("nbytes to write this time: %d.\n", nbyte);
            memcpy(block, ptr, nbyte);

            if (! back_store_write(fs->bs, nextID, ptr))
            {
                //printf("Failed to write last part to BS.\n");
                return -1;
            }

            d->offset += nbyte;
            nbyte = 0;
        }
    }

    int bytesWritten = totalNumberOfBytesOriginally;

    //Adjust actual file size
    uint32_t newFileSize = ceil((float)d->offset / 1024.0) * 1024;
    if (newFileSize > record->metadata.fileSize)
    {
            record->metadata.fileSize = newFileSize;
    }

    //Adjust virtual file size
    record->metadata.virtualFileSize = (newFileSize - 1024) + (d->offset % 1024);

    printf("New filesize: %d.\n", record->metadata.fileSize);
    printf("New offset: %d.\n", d->offset);
    printf("New virtual size: %d.\n", record->metadata.virtualFileSize);

    return bytesWritten;

    /*
    int totalNumberOfBytesOriginally = nbyte;
    uint8_t *ptr = (uint8_t *)src;

    /////////////////////
    //Validate Parameters

    if (! fs || fd < 0 || fd >= _MAX_NUM_OPEN_FILES || ! src)
    {
        printf("Invalid parameters passed to fs_write.\n");
        return -1;
    }

    printf("fs_write called with fs: %p, fd: %d, src: %p, nbyte: %d.\n", fs, fd, src, nbyte);

    ////////////////////////////
    //Write To End of File First

    //get the file from file descriptors
    FileDes_t *d = &fs->file_descriptors[fd];

    if (d->file_record_index == 0)
    {
        printf("File not currently opened!\n");
        return -2;
    }

    file_record_t *record = &fs->file_records[d->file_record_index];

    if (record->name[0] == '\0')
    {
        printf("Invalid file record detected.\n");
        return -3;
    }

    //determine whether necessary to write data to the remainder of the current block where the offset is at
    if (d->offset % 1024 != 0)
    {
        //not at the end of the last written block, so finish block out
        int lastWrittenBlockIndex = get_block_bs_index(fs, record, d->offset);
        int startIndexWithinBlock = d->offset % 1024;

        //grab its final block
        uint8_t block[1024] = {0};

        if (! back_store_read(fs->bs, lastWrittenBlockIndex, block))
        {
            printf("Failed to read from back store.\n");
            return -4;
        }

        //write to that block's end
        int numBytesToWrite = 1024 - startIndexWithinBlock;
        memcpy(block, ptr, numBytesToWrite);

        ptr += numBytesToWrite; //keep it moving forward

        //push block back to backing_store
        if (! back_store_write(fs->bs, lastWrittenBlockIndex, block))
        {
            printf("Failed to write to back store.\n");
            return -5;
        }

        //update num bytes left
        nbyte -= numBytesToWrite;
        d->offset += numBytesToWrite;
    }


    //now write until the end of the current file size
    while (d->offset < record->metadata.fileSize && nbyte > 0)
    {
        uint8_t block[1024] = {0};
        int bsBlockIndex = get_block_bs_index(fs, record, d->offset);

        if (bsBlockIndex < 0)
        {
            printf("Failed find bsBlockIndex.\n");
            return -10;
        }

        if (nbyte > 1024)
        {
            if (! back_store_write(fs->bs, bsBlockIndex, ptr))
            {
                printf("Failed to read from back_store.\n");
                return -11;
            }

            nbyte -= 1024;
            ptr += 1024;
            d->offset += 1024;
        }
        else
        {
            memcpy(block, ptr, nbyte);
            ptr += nbyte;

            if (! back_store_write(fs->bs, bsBlockIndex, block))
            {
                printf("Failed write block to back store.\n");
                return -12;
            }

            nbyte = 0;
            d->offset += nbyte;

            //no more data to write so return
            return totalNumberOfBytesOriginally;
        }
    }

    ///////////////////////////////
    //Allocate Needed Memory Blocks

    //allocate all remaining blocks needed

    //calculate number of needed blocks based on remaining bytes
    int numNeededDataBlocks = ceil((float)nbyte / (float)1024);

    //calculate number of needed file blocks
    int needIndirectBlock = 0;
    int numCurrentBlocks = d->offset / 1024;
    int numBlocksNeededTotal = numCurrentBlocks + numNeededDataBlocks;

    //if currently in D range and need to move to ID range
    if (numCurrentBlocks < 7 && numBlocksNeededTotal >= 7)
    {
        //need to allocate an indirect block
        needIndirectBlock = 1;
    }

    int needDoubleIndirectBlock = 0;

    //if currently in ID range and need to move to DID range
    if (numCurrentBlocks < 518 && numBlocksNeededTotal >= 518)
    {
        //need a double indirect block
        needDoubleIndirectBlock = 1;
    }

    int numIDNeededForDIDRange = 0; //subtract 518 to remove blocks covered in direct and indirect ranges

    if (numBlocksNeededTotal >= 519)
    {
        //need to consider additional ID blocks in tbe DID block
        //total - current = needed ID blocks
        int totalIDNeededForDIDRange = ceil((float)(numBlocksNeededTotal - 518) / (float)512); //remove blocks covered by D and ID sections

        if (numCurrentBlocks > 518)
        {
            //need to remove current ID's already allocated
            int currentNumIDNeededForDIDRange = ceil((float)numCurrentBlocks - 518 / (float)512);
            numIDNeededForDIDRange = totalIDNeededForDIDRange - currentNumIDNeededForDIDRange;
        }
    }

    //allocate each file block, storing its location
    int numFileBlocksNeeded = needDoubleIndirectBlock + needDoubleIndirectBlock + numIDNeededForDIDRange;
    int *fileBlocks = (int *)malloc(sizeof(int) * numFileBlocksNeeded);

    //attempt to allocate the file blocks
    for (int i = 0; i < numFileBlocksNeeded; ++i)
    {
        fileBlocks[i] = back_store_allocate(fs->bs);

        if (fileBlocks[i] == 0)
        {
            printf("Failed to allocate memory from the backing store.\n");
            deallocate_bs_index_array(fs->bs, fileBlocks, i - 1); //go to i - 1 because not all blocks have been allocated
            free(fileBlocks);
            return -40;
        }
    }

    //create an array of the proper size
    int *newDataBlockIndexes = (int *)malloc(sizeof(int) * numNeededDataBlocks);

    if (! newDataBlockIndexes)
    {
        printf("Ran out of memory for making block id array.\n");
        return -6;
    }

    //allocate each data block, storing its location
    for (int i = 0; i < numNeededDataBlocks; ++i)
    {
        newDataBlockIndexes[i] = back_store_allocate(fs->bs);

        if (newDataBlockIndexes[i] == 0)
        {
            //back_store failed to allocate memory
            printf("Failed to allocate enough memory from the back_store.\n");
            deallocate_bs_index_array(fs->bs, fileBlocks, numFileBlocksNeeded);
            deallocate_bs_index_array(fs->bs, newDataBlockIndexes, i - 1); //not all of them have been allocated yet...
            free(newDataBlockIndexes);
            free(fileBlocks);
            return -7;
        }
    }

    //////////////////////////////////////////////
    //Setup FileSystem Data Framework (ID and DID)
    {
        //hold a current location index
        int i = 0;

        //allocate ID
        if (needIndirectBlock)
        {
            record->block_refs[6] = fileBlocks[i++];
        }

        //allocate DID
        if (needDoubleIndirectBlock)
        {
            record->block_refs[7] = fileBlocks[i++];
        }

        //allocate ID's in DID
        if (numIDNeededForDIDRange > 0)
        {
            //open data block
            uint16_t IDBlock[512] = {0};

            if (! back_store_read(fs->bs, record->block_refs[7], IDBlock))
            {
                printf("Failed to read data for ID blocks from a file's given DID block index.\n");
                deallocate_bs_index_array(fs->bs, &fileBlocks[i], numFileBlocksNeeded - i); //free up to what has not been assigned
                free(fileBlocks);

                deallocate_bs_index_array(fs->bs, newDataBlockIndexes, numNeededDataBlocks);
                free(newDataBlockIndexes);
                return -50;
            }

            //find start of non-allocated numbers
            int j = 0;

            for ( ; j < 512; ++j)
            {
                if (IDBlock[j] == 0)
                {
                    //found it!
                    IDBlock[j] = fileBlocks[i++];
                }

                if (i == numIDNeededForDIDRange)
                {
                    //already allocated all of the needed slots
                    break;
                }
            }
        }

        //free file system array
        free(fileBlocks);
    }

    ////////////////////////////////////////////
    //Write Remaining Data to Blocks, one by one

    uint8_t block[1024] = {0};

    for (int i = 0; i < numNeededDataBlocks; ++i)
    {
        if (! back_store_read(fs->bs, newDataBlockIndexes[i], block))
        {
            printf("Failed to read from the backing_store!\n");
            deallocate_bs_index_array(fs->bs, newDataBlockIndexes, numNeededDataBlocks);
            free(newDataBlockIndexes);
            return -8;
        }

        if (nbyte > 1024)
        {
            //write entire block
            memcpy(block, src, 1024);
            nbyte -= 1024;
        }
        else
        {
            //write what's left
            memcpy(block, src, nbyte);
            nbyte = 0;
        }
    }

    ////////////////////////////
    //Link New Blocks up to File

    //grab the file from the file descriptors
    //stored in record ^^^

    //calculate number of data blocks needed
    //TODO: Fix
    int currentBlockIndex = record->metadata.fileSize / 1024; //add one to get to first non-used block

    for (int i = 0; i < numNeededDataBlocks; ++i)
    {
        //link it up to the next location in the file
        if (currentBlockIndex < 6)
        {
            //in fast portion of file system
            record->block_refs[currentBlockIndex] = newDataBlockIndexes[i]; //simple, just copy value
        }
        else if (currentBlockIndex <= 517)
        {
            //indirect
            //more difficult...
            int blockOfDirectLinksIndex = currentBlockIndex - 6;

            //grab block of direct links
            if (! back_store_read(fs->bs, record->block_refs[6], block))
            {
                printf("Failed to read in block of direct links from back_store.\n");
                deallocate_bs_index_array(fs->bs, newDataBlockIndexes, numNeededDataBlocks);
                free(newDataBlockIndexes);
                return -31;
            }

            uint16_t *directLinks = (uint16_t *)block;
            directLinks[blockOfDirectLinksIndex] = newDataBlockIndexes[i];
        }
        else if (currentBlockIndex <= 262661)
        {
            //double indirect
            int indirectBlockIndex = (currentBlockIndex - 518) / 512; //subtract 518 to remove direct and indirect block locations and divide by 512 to ensure proper one received
            int directBlockIndex = (currentBlockIndex - 518) % 512;
            currentBlockIndex -= 518;

            if (! back_store_read(fs->bs, record->block_refs[7], block))
            {
                printf("Failed to read in block of indirect links from back_store.\n");
                deallocate_bs_index_array(fs->bs, newDataBlockIndexes, numNeededDataBlocks);
                free(newDataBlockIndexes);
                return -32;
            }

            //grab indirect links
            uint16_t *doubleIndirectLinks = (uint16_t *)block;

            //figure out indirect link from double indirect and load block of direct links
            if (! back_store_read(fs->bs, doubleIndirectLinks[indirectBlockIndex], block))
            {
                printf("Failed to read in block of indirect links from back_store.\n");
                deallocate_bs_index_array(fs->bs, newDataBlockIndexes, numNeededDataBlocks);
                free(newDataBlockIndexes);
                return -32;
            }

            //figure out direct link from indirect
            uint16_t *directLinks = (uint16_t *)block;
            directLinks[directBlockIndex] = newDataBlockIndexes[i]; //found the link, so set it to the proper location
        }
        else
        {
            printf("Whelp. This sucks.\n");
            deallocate_bs_index_array(fs->bs, newDataBlockIndexes, numNeededDataBlocks);
            free(newDataBlockIndexes);
            return -30;
        }

        currentBlockIndex++; //do the next block linkage
    }

    ////////////////////////////
    //Adjust Filesystem Metadata

    record->metadata.fileSize += numNeededDataBlocks * 1024; //include actual file size
    d->offset += totalNumberOfBytesOriginally;

    /////////////
    //Free Memory

    free(newDataBlockIndexes);

    ////////////////////////////////
    //Return number of bytes written

    return totalNumberOfBytesOriginally;
    */
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
    //printf("*** fs_remove called...");

    /////////////////////
    //Validate Parameters

    if (! fs || ! path)
    {
        //printf("Invalid parameters passed to fs_remove.\n");
        return -1;
    }

    //printf("Removing: %s\n", path);

    /////////////////////////////////////////
    //Delete Specified File From File Records

    //find the file
    int fileIndex = fs_traverse(fs, path);

    if (fileIndex == 0)
    {
        //can't do that!
        //printf("Cannot delete the root directory!\n");
        return -1;
    }

    if (fileIndex == -1)
    {
        //printf("Couldn't find file: %s.\n", path);
        return -1;
    }

    //printf("Index of file: %d.\n", fileIndex);

    //if directory and the directory is not empty, then report an error
    file_record_t *fr = &fs->file_records[fileIndex];

    if (fr->type == FS_DIRECTORY)
    {
        //printf("Trying to delete directory... Need to determine if empty.\n");

        //pull block
        directory_t block;

        if (! back_store_read(fs->bs, fr->block_refs[0], &block))
        {
            //error reading from the back store
            //printf("Error: Couldn't read from the back_store.\n");
            return -2;
        }

        //look to see if directory is empty
        for (int i = 0; i < 15; ++i)
        {
            if (block.entries[i].file_record_index != 0)
            {
                //found a non empty directory!
                //printf("Error: Cannot delete a non-empty directory.\n");
                return -3;
            }
        }

        //printf("Directory is empty.\n");
    }

    //made it this far, so the directory must be empty

    //free all blocks in the file, increment by 1024 to grab one new block per iteration
    int numBlocksInFile = ceil(fr->metadata.fileSize / 1024.0);

    for (int j = 0; j < numBlocksInFile; j += 1)
    {
        //get the block index associated with the file offset j
        int bsIndex = get_block_bs_index(fs, fr, j);

        //printf("Releasing at BS Index: %d.\n", bsIndex);

        if (bsIndex < 0)
        {
            //error occurred getting the block
            //printf("Error occurred getting the block.\n");
            return -4;
        }

        //free the block memory
        back_store_release(fs->bs, bsIndex);
    }

    //zero the file record out in the file system
    uint8_t zero_mem[128] = {0};
    memcpy(fr, zero_mem, 128);

    ///////////////////////////////////////////
    //Close All File Descriptors for Given File

    //search through in a for loop for indexes == fileIndex and close

    for (int j = 0; j < _MAX_NUM_OPEN_FILES; ++j)
    {
        if (fs->file_descriptors[j].file_record_index == fileIndex)
        {
            //found an open file descriptor
            //printf("Closing open file index at: %d.\n", j);
            fs_close(fs, j); //<< Close the file descriptor
        }
    }

    ///////////////////////////////////
    //Remove File from Parent Directory

    //grab the parent directory
    int parentDirectoryIndex = 0;

    {
        //remove last slash to find full parent directory path

        char *pathDup = strdup(path);
        char *parentDirPath;
        char *c = pathDup;
        while (*c != '\0')
            c++;
        while (*c != '/')
            c--;
        *c = '\0';
        parentDirPath = strdup(pathDup);
        //printf("Retrieving parent directory at %s.\n", parentDirPath);
        free(pathDup);
        parentDirectoryIndex = fs_traverse(fs, parentDirPath);
        //printf("Results of search: %d.\n", parentDirectoryIndex);
        free(parentDirPath);
    }

    //open directory data block
    uint8_t block[1024] = {0};

    {
        file_record_t *pd = &fs->file_records[parentDirectoryIndex];

        if (! back_store_read(fs->bs, pd->block_refs[0], &block))
        {
            //printf("Failed to read from backstore in fs_release.\n");
            return -5;
        }
    }

    //search through directory and remove file stub from parent directory
    {
        directory_t *dir = (directory_t *)block;

        int i = 0;
        for ( ; i < 15; ++i)
        {
            if (dir->entries[i].file_record_index == fileIndex)
            {
                //found the index of the file to delete
                //zero out entry
                uint8_t zeroMem[sizeof(directory_entry_t)] = {0}; //create block of zeroed out memory
                memcpy(&dir->entries[i], zeroMem, sizeof(directory_entry_t));

                //push block to back_store
                file_record_t *pd = &fs->file_records[parentDirectoryIndex];

                if (! back_store_write(fs->bs, pd->block_refs[0], &block))
                {
                    //printf("Failed to write directory back to the back_store.\n");
                    return -6;
                }

                //done searching
                break;
            }
        }

        if (i == 15)
        {
            //printf("Failed to find file to remove in parent directory.\n");
            return -7;
        }
    }

    ////////////////
    //Signal success

    return 0;
}

//////////////////
// Milestone 3 ///
//////////////////

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
    if (! fs || fd < 0 || fd >= _MAX_NUM_OPEN_FILES)
    {
        printf("Invalid fs or fd.\n");
        return -1;
    }

    //TODO: Check offset and whence
    if (offset == 0 || (whence != FS_SEEK_CUR && whence != FS_SEEK_END && whence != FS_SEEK_SET))
    {
        printf("Invalid offset or seek.\n");
        return -2;
    }

    FileDes_t *d = fs->file_descriptors[fd];
    file_record_t *rec = fs->file_records[d->file_record_index];

    if (whence == FS_SEEK_CUR)
    {
        d->offset += offset;
    }
    else if (whence == FS_SEEK_END)
    {
        d->offset = rec->metadata.virtualFileSize + offset;
    }
    else
    {
        //adjust based on beginning of file
        d->offset = offset;
    }

    //catch out of bounds
    if (d->offset < 0)
    {
        d->offset = 0;
    }
    else if (d->offset > rec->metadata.virtualFileSize)
    {
        d->offset = rec->metadata.virtualFileSize;
    }

    return d->offset;
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
    /////////////////////
    //Validate parameters

    if (! fs || fd < 0 || fd >= _MAX_NUM_OPEN_FILES || ! dst)
    {
        printf("Invalid parameters detected.\n");
        return -1;
    }

    if (nbyte == 0)
    {
        printf("Returning 0 bytes...");
        return 0;
    }

    ////////////////////////////
    //Read to end of first block

    //track nbytes started with to calculated final offset
    int startingNByte = nbyte;

    //get the file
    FileDes_t *d = &fs->file_descriptors[fd];
    file_record_t *rec = &fs->file_records[d->file_record_index];

    //calculate the offset block index and offset inside of block
    int blockIndex = d->offset / 1024;
    int offset = d->offset % 1024;
    int numBytesToReadInBlock = 1024 - offset;
    //int numBytesToReadInBlock = rec->metadata.virtualFileSize - (blockIndex * 1024 + offset) + 1;

    //check to make sure that you don't go "overboard"
    if (blockIndex * 1024 + numBytesToReadInBlock > rec->metadata.virtualFileSize)
    {
        numBytesToReadInBlock = rec->metadata.virtualFileSize - 1024 * blockIndex;
    }

    //further restrain to only the number of bytes requested
    if (numBytesToReadInBlock > nbyte)
    {
        numBytesToReadInBlock = nbyte;
    }

    //retrieve and read until end of block
    int bsBlockIndex = get_block_bs_index(fs, rec, blockIndex);

    if (blockIndex == -1)
    {
        printf("Failed to get the block index.\n");
        return -2;
    }

    uint8_t block[1024];

    if (! back_store_read(fs->bs, bsBlockIndex, &block))
    {
        printf("Failed to get the block data at index: %d.\n", bsBlockIndex);
        return -3;
    }

    uint8_t *ptr = (uint8_t *)dst;

    memcpy(dst, ptr, numBytesToReadInBlock);

    ptr += numBytesToReadInBlock;
    nbyte -= numBytesToReadInBlock;

    if (nbyte == 0)
    {
        d->offset += numBytesToReadInBlock; //keep track of this
        return numBytesToReadInBlock;
    }

    /////////////////////////////////////////////////////////////////////
    //Read rest of bytes of all following blocks until EOF or nbytes == 0

    int bytesLeftUntilEOF = rec->metadata.virtualFileSize - d->offset;

    if (nbyte > bytesLeftUntilEOF)
    {
        nbyte = bytesLeftUntilEOF;
    }

    while (nbyte > 0 && d->offset < rec->metadata.virtualFileSize)
    {
        blockIndex++; //go to next file block index

        bsBlockIndex = get_block_bs_index(fs, rec, blockIndex);

        if (! back_store_read(fs->bs, bsBlockIndex, &block))
        {
            printf("Failed to get the block data at index: %d.\n", bsBlockIndex);
            return -4;
        }

        if (nbyte >= 1024)
        {
            memcpy(dst, block, 1024);
            nbyte -= 1024;
            ptr += 1024;
            d->offset += nbyte;
        }
        else
        {
            //nbyte < 1024
            memcpy(dst, block, nbyte);
            ptr += nbyte;
            d->offset += nbyte;
            nbyte = 0;
        }
    }

    /////////////////////////////
    //Return number of bytes read

    return startingNByte;
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

    //open directory
    int dirIndex = fs_traverse(fs, path);

    if (dirIndex == -1)
    {
        printf("Directory doesn't exist!\n");
        return NULL;
    }

    directory_t dir;

    if (! back_store_read(fs->bs, dirIndex, &dir);)
    {
        printf("Failed to read from back_store.\n");
        return NULL;
    }

    //create dyn_array
    dyn_array_t *da = dyn_array_create(0, sizeof(file_record_t), NULL);

    //for each in direct
    for (int i = 0; i < 15; ++i)
    {
        directory_entry_t entry = dir.entries[i];

        if (entry.file_name[0] != '\0')
        {
            //found a live one! Make sure to grab it....
            if (! dyn_array_push_back(da, &fs->file_records[entry.file_record_index]))
            {
                printf("Failed to push directory object to back of dyn_array.\n");
                dyn_array_destroy(da);
                return NULL;
            }
        }
    }

    //successful in creating dyn_array object
    return da;
}

/////////////////////////
// Extra Credit!!! :D ///
/////////////////////////

/*
    Gets the parent directory of a given path.

    Returns NULL if failed or the parent dir on success
*/
static char *get_parent_dir(const char *path)
{
    if (! path)
    {
        return NULL;
    }

    if (path == '/')
    {
        return NULL;
    }

    char *p = strdup(path);

    char *c = *p;

    while (*c != '\0')
    {
        c++;
    }

    while (*c != '/')
    {
        c--;
    }

    *c = '\0';

    char *parentDir = strdup(p);

    free(p);

    return parentDir;
}

/*
    Gets the filename for a given path and returns it
    Returns NULL on failure
*/
static char *get_filename(char *path)
{
    if (! path)
    {
        return NULL;
    }

    if (path == '/')
    {
        return NULL;
    }

    char *p = strdup(path);

    char *c = *p;

    while (*c != '\0')
    {
        c++;
    }

    while (*c != '/')
    {
        c--;
    }

    *c = '\0';

    char *filename = strdup(c + 1);

    free(p);

    return filename;
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
    /////////////////////
    //Validate Parameters

    //ensure that the parameters are not NULL
    if (! fs || ! src || ! dst)
    {
        printf("Invalid parameters.\n");
        return -1;
    }

    //ensure that you're not trying to move a directory within itself

    if (strstr(dst, src) != NULL)
    {
        printf("Can't move a directory to within itself.\n");
        return -2;
    }

    //TODO: Validate that the parent directory has a spot open for additions

    ////////////////////
    //Move the directory

    //grab this for the destination directory addition later on
    int srcFileIndex = fs_traverse(fs, src);

    if (srcFileIndex == -1)
    {
        printf("Couldn't open the srcFileIndex.\n");
        return -9;
    }

    //remove from src
    char *srcParentDir = get_parent_dir(src);

    int srcParentDirFileIndex = fs_traverse(fs, srcParentDir);
    free(srcParentDir); //don't need this anymore

    if (srcParentDirFileIndex == -1)
    {
        printf("Failed to get parent directory.\n");
        return -3;
    }

    //search through src directory for file
    directory_t dir;
    file_record_t *parentDirRecord = &fs->file_records[srcParentDirFileIndex];

    if (! back_store_read(fs->bs, parentDirRecord->block_refs[0], &dir))
    {
        printf("Failed to get the parent directory data.\n");
        return -4;
    }

    //get the filename to search for
    char *srcFileName = get_filename(src);

    //search through all of the directory's listings
    int i = 0;
    for ( ; i < 15; ++i)
    {
        directory_entry_t *entry = &dir.entries[i];

        //if this is the file we're looking for, then we need to remove it from the directory
        //It will be added later on in fs_move to the other directory
        if (strcmp(entry.file_name, srcFileName) == 0)
        {
            //found the file
            //remove the file from this location
            entry->file_name = '\0';
            entry->file_record_index = 0;
            free(srcFileName); //this is no longer needed...

            //write updated directory data back to the BS
            if (! back_store_write(fs->bs, parentDirRecord->block_refs[0]))
            {
                printf("Failed to write back to parent directory location.\n");
                return -5;
            }

            break;
        }
    }

    if (i == 15)
    {
        printf("Couldn't find the file specified.\n");
        return -6;
    }

    //ok, so by this point everything should be removed from the src parent directory.
    //All we need to do now is add it to the other directory by its new name

    //add to dst with new filename
    char *dstParentDir = get_parent_dir(dst);

    int dstParentDirFileIndex = fs_traverse(fs, dstParentDir);
    free(dstParentDir); //don't need this anymore

    if (dstParentDirFileIndex == -1)
    {
        printf("Failed to get parent directory for the dst.\n");
        return -7;
    }

    //search through src directory for file
    //use the previously defined dir
    file_record_t *dstParentDirRecord = &fs->file_records[srcParentDirFileIndex];

    if (! back_store_read(fs->bs, dstParentDirRecord->block_refs[0], &dir))
    {
        printf("Failed to get the parent directory data.\n");
        return -8;
    }

    //get the filename to search for
    char *dstFileName = get_filename(dst);

    //search through all of the directory's listings
    i = 0;
    for ( ; i < 15; ++i)
    {
        directory_entry_t *entry = &dir.entries[i];

        //If this is an empty location then we need to add the file here so it shows up in the directory.
        if (entry.file_name[0] == '\0')
        {
            //found an open location
            //add the file to this location
            strcpy(entry->file_name, dstFileName);
            entry->file_record_index = srcFileIndex;
            free(dstFileName); //this is no longer needed...

            //write updated directory data back to the BS
            if (! back_store_write(fs->bs, parentDirRecord->block_refs[0]))
            {
                printf("Failed to write back to parent directory location.\n");
                return -9;
            }

            break;
        }
    }

    //i==15 case (dst directory is full) is handled at the beginning of the function
    //Should check this before deleting the file from it's original directory (that's why it's up there)

    //////////////////
    //Return "Success"

    return 0;
}
