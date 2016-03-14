#include "../include/back_store.h"
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <bitmap.h>
#include <unistd.h>
#include <stdio.h>

//this holds the total number of blocks that will be stored in the file system
#define NUM_BLOCKS 65536

//holds the zero based inex of the first byte of the block data
#define FIRST_BLOCK 8192

#define BLOCK_SIZE 1024

// Back store object
struct back_store {
    bitmap_t* bitmap; //holds the information on available bitmaps
    int loc; //holds the file id
};

///
/// Creates a new back_store file at the specified location
///  and returns a back_store object linked to it
/// \param fname the file to create
/// \return a pointer to the new object, NULL on error
///
back_store_t *back_store_create(const char *const fname) {
    if (! fname) {
        return NULL;
    }

    int file = open(fname, O_RDWR | O_CREAT | O_SYNC | O_TRUNC, 0666);

    if (file == -1) {
        //failed to open file...
        return NULL;
    }

    back_store_t *ptr = (back_store_t *) malloc(sizeof(back_store_t));

    //create the bitmap
    ptr->bitmap = bitmap_create(NUM_BLOCKS);

    //set all of the bitmap to 1
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        bitmap_set(ptr->bitmap, i);
    }

    //signal the first 8 blocks as used (first 8 blocks store the bitmap)
    for (size_t i = 0; i < 8; ++i) {
        bitmap_reset(ptr->bitmap, i);
    }

    //write backing store data to file
    //first 8 blocks will contain bitmap
    uint8_t *data = (uint8_t *) bitmap_export(ptr->bitmap);
    int numBytes = bitmap_get_bytes(ptr->bitmap);
    write(file, data, numBytes); //write the data to the beginning of the file
    ptr->loc = file;

    return ptr;
}

///
/// Opens the specified back_store file
///  and returns a back_store object linked to it
/// \param fname the file to open
/// \return a pointer to the new object, NULL on error
///
back_store_t *back_store_open(const char *const fname) {
    if (! fname) {
        return NULL;
    }

    int file = open(fname, O_RDONLY);

    if (file == -1) {
        //problem detected
        return NULL;
    }

    //read in bitmap data
    uint8_t bmData[8192];
    pread(file, bmData, 8192, 0); //read in beginning of file

    //create the backing store
    back_store_t *bs = (back_store_t *) malloc(sizeof(back_store_t));
    bs->bitmap = bitmap_import(NUM_BLOCKS, bmData);
    bs->loc = file;

    return bs;
}

///
/// Closes and frees a back_store object
/// \param bs block_store to close
///
void back_store_close(back_store_t *const bs) {
    if (! bs) {
        return;
    }

    //write bitmap to file
    uint8_t *bmData = (uint8_t *) bitmap_export(bs->bitmap);
    pwrite(bs->loc, bmData, 8192, 0);

    //close file handle
    close(bs->loc);

    //destroy the bitmap in memory
    bitmap_destroy(bs->bitmap);

    //free memory
    free(bs);
}

///
/// Allocates a block of storage in the back_store
/// \param bs the back_store to allocate from
/// \return id of the allocated block, 0 on error
///
unsigned back_store_allocate(back_store_t *const bs) {
    if (! bs) {
        return 0;
    }

    size_t loc = bitmap_ffs(bs->bitmap); //find the first free location

    if (loc == SIZE_MAX) {
        return 0; //couldn't find an open location
    }

    //else, allocate memory there
    back_store_request(bs, loc);

    return loc;
}

///
/// Requests the allocation of a specified block id
/// \param bs back_store to allocate from
/// \param block_id block to attempt to allocate
/// \return bool indicating allocation success
///
bool back_store_request(back_store_t *const bs, const unsigned block_id) {
    //validate proper allocation
    if (! bs) {
        return false;
    }

    //check limits of block_id
    if (block_id < 8 || block_id > NUM_BLOCKS) {
        return false;
    }

    if (bitmap_test(bs->bitmap, block_id) == 0) {
        //already allocated, so don't reallocate
        return false;
    }

    //else, not allocated, so good to go!
    uint8_t blockData[BLOCK_SIZE] = { 0 };

    //initialize block
    lseek(bs->loc, FIRST_BLOCK + (BLOCK_SIZE * block_id), SEEK_SET);
    write(bs->loc, blockData, BLOCK_SIZE);

    //set memory map to note that the memory is in use
    bitmap_reset(bs->bitmap, block_id);

    return true;
}

///
/// Releases the specified block id so it may be used later
/// \param bs back_store object
/// \param block_id block to release
///
void back_store_release(back_store_t *const bs, const unsigned block_id) {
    if (! bs) {
        return;
    }

    bitmap_set(bs->bitmap, block_id);
}

///
/// Reads data from the specified block to the given data buffer
/// \param bs the object to read from
/// \param block_id the block to read from
/// \param dst the buffer to write to
/// \return bool indicating success
///
bool back_store_read(back_store_t *const bs, const unsigned block_id, void *const dst) {
    if (! bs || ! dst) {
        return false;
    }

    if (block_id < 8 || block_id >= NUM_BLOCKS) {
        //trying to access protected memory
        return false;
    }

    if (bitmap_test(bs->bitmap, block_id) == true) {
        //the location is not yet set
        return false;
    }

    //move to correct pointer location
    lseek(bs->loc, FIRST_BLOCK + (BLOCK_SIZE * block_id), SEEK_SET);

    //read in data
    read(bs->loc, dst, BLOCK_SIZE);

    return true;
}

///
/// Writes data from the given buffer to the specified block
/// \param bs the object to write to
/// \param block_id the block to write to
/// \param src the buffer to read from
/// \return bool indicating success
///
bool back_store_write(back_store_t *const bs, const unsigned block_id, const void *const src) {
    if (! bs || ! src) {
        return false;
    }

    if (block_id < 8 || block_id >= NUM_BLOCKS) {
        //trying to access protected memory
        return false;
    }

    //go to proper location
    lseek(bs->loc, FIRST_BLOCK + (BLOCK_SIZE * block_id), SEEK_SET);

    //write data
    write(bs->loc, src, BLOCK_SIZE);

    return true;
}
