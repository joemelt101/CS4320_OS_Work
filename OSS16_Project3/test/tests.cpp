#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>
using std::vector;
using std::string;

#include <gtest/gtest.h>

extern "C" {
#include "../src/S16FS.c"
}

unsigned int score;
unsigned int max;

class GradeEnvironment : public testing::Environment {
  public:
    virtual void SetUp() {
        score = 0;
        max   = 80;
    }
    virtual void TearDown() { std::cout << "SCORE: " << score << " (out of " << max << ")" << std::endl; }
};

bool find_in_directory(const dyn_array_t *const record_arr, const char *fname) {
    if (record_arr && fname) {
        for (size_t i = 0; i < dyn_array_size(record_arr); ++i) {
            if (strncmp(((file_record_t *) dyn_array_at(record_arr, i))->name, fname, FS_FNAME_MAX) == 0) {
                return true;
            }
        }
    }
    return false;
}

/*

S16FS_t * fs_format(const char *const fname);
    1   Normal
    2   NULL
    3   Empty string

S16FS_t *fs_mount(const char *const fname);
    1   Normal
    2   NULL
    3   Empty string

int fs_unmount(S16FS_t *fs);
    1   Normal
    2   NULL

*/

TEST(a_tests, format_mount_unmount) {
    const char *test_fname = "a_tests.s16fs";

    S16FS_t *fs = NULL;

    // FORMAT 2
    ASSERT_EQ(fs_format(NULL), nullptr);

    // FORMAT 3
    // this really should just be caught by back_store
    ASSERT_EQ(fs_format(""), nullptr);

    // FORMAT 1
    fs = fs_format(test_fname);
    ASSERT_NE(fs, nullptr);

    // UNMOUNT 1
    ASSERT_EQ(fs_unmount(fs), 0);

    // UNMOUNT 2
    ASSERT_LT(fs_unmount(NULL), 0);

    // MOUNT 1
    fs = fs_mount(test_fname);
    ASSERT_NE(fs, nullptr);
    fs_unmount(fs);

    // MOUNT 2
    ASSERT_EQ(fs_mount(NULL), nullptr);

    // MOUNT 3
    // If you get weird behavior here, update/reinstall back_store if you haven't
    // There was a bug where it would try to open files with O_CREATE
    // Which, obviously, would cause issues
    ASSERT_EQ(fs_mount(""), nullptr);

    score += 1;
}

/*

int fs_create(S16FS_t *const fs, const char *const fname, const ftype_t ftype);
    1. Normal, file, in root
    2. Normal, directory, in root
    3. Normal, file, not in root
    4. Normal, directory, not in root
    5. Error, NULL fs
    6. Error, NULL fname
    7. Error, empty fname
    8. Error, bad type
    9. Error, path does not exist
    10. Error, Root clobber
    11. Error, already exists
    12. Error, file exists
    13. Error, part of path not directory
    14. Error, path terminal not directory
    15. Error, path string has no leading slash
    16. Error, path has trailing slash (no name for desired file)
    17. Error, bad path, path part too long
    18. Error, bad path, desired filename too long
    19. Error, directory full.
    20. Error, out of inodes.
    21. Error, out of data blocks & file is directory (requires functional write)

*/

TEST(b_tests, file_creation_one) {
    vector<const char *> filenames{
        "/file", "/folder", "/folder/with_file", "/folder/with_folder", "/DOESNOTEXIST", "/file/BAD_REQUEST",
        "/DOESNOTEXIST/with_file", "/folder/with_file/bad_req", "folder/missing_slash", "/folder/new_folder/",
        "/folder/withwaytoolongfilenamethattakesupmorespacethanitshould and yet was not enough so I had to add "
        "more/bad_req",
        "/folder/withfilethatiswayyyyytoolongwhydoyoumakefilesthataretoobigEXACT!", "/", "/mystery_file"};

    const char *test_fname = "b_tests_normal.s16fs";

    S16FS_t *fs = fs_format(test_fname);

    ASSERT_NE(fs, nullptr);

    // CREATE_FILE 1
    ASSERT_EQ(fs_create(fs, filenames[0], FS_REGULAR), 0);

    // CREATE_FILE 2
    ASSERT_EQ(fs_create(fs, filenames[1], FS_DIRECTORY), 0);

    // CREATE_FILE 3
    ASSERT_EQ(fs_create(fs, filenames[2], FS_REGULAR), 0);

    // CREATE_FILE 4
    ASSERT_EQ(fs_create(fs, filenames[3], FS_DIRECTORY), 0);

    // CREATE_FILE 5
    ASSERT_LT(fs_create(NULL, filenames[4], FS_REGULAR), 0);

    // CREATE_FILE 6
    ASSERT_LT(fs_create(fs, NULL, FS_REGULAR), 0);

    // CREATE_FILE 7
    ASSERT_LT(fs_create(fs, "", FS_REGULAR), 0);

    // CREATE_FILE 8
    ASSERT_LT(fs_create(fs, filenames[13], (file_t) 44), 0);

    // CREATE_FILE 9
    ASSERT_LT(fs_create(fs, filenames[6], FS_REGULAR), 0);

    // CREATE_FILE 10
    ASSERT_LT(fs_create(fs, filenames[12], FS_DIRECTORY), 0);

    // CREATE_FILE 11
    ASSERT_LT(fs_create(fs, filenames[1], FS_DIRECTORY), 0);
    ASSERT_LT(fs_create(fs, filenames[1], FS_REGULAR), 0);

    // CREATE_FILE 12
    ASSERT_LT(fs_create(fs, filenames[0], FS_REGULAR), 0);
    ASSERT_LT(fs_create(fs, filenames[0], FS_DIRECTORY), 0);

    // CREATE_FILE 13
    ASSERT_LT(fs_create(fs, filenames[5], FS_REGULAR), 0);

    // CREATE_FILE 14
    ASSERT_LT(fs_create(fs, filenames[7], FS_REGULAR), 0);

    // CREATE_FILE 15
    ASSERT_LT(fs_create(fs, filenames[8], FS_REGULAR), 0);
    // But if we don't support relative paths, is there a reason to force abolute notation?
    // It's really a semi-arbitrary restriction
    // I suppose relative paths are up to the implementation, since . and .. are just special folder entires
    // but that would mess with the directory content total, BUT extra parsing can work around that.
    // Hmmmm.

    // CREATE_FILE 16
    ASSERT_LT(fs_create(fs, filenames[9], FS_DIRECTORY), 0);

    // CREATE_FILE 17
    ASSERT_LT(fs_create(fs, filenames[10], FS_REGULAR), 0);

    // CREATE_FILE 18
    ASSERT_LT(fs_create(fs, filenames[11], FS_REGULAR), 0);

    // Closing this file now for inspection to make sure these tests didn't mess it up

    fs_unmount(fs);

    score += 1;
}

TEST(b_tests, file_creation_two) {
    // CREATE_FILE 19 - OUT OF INODES (and test 18 along the way)
    // Gotta make... Uhh... A bunch of files. (255, but we'll need directories to hold them as well)

    const char *test_fname = "b_tests_full_table.s16fs";
    S16FS_t *fs            = fs_format(test_fname);

    ASSERT_NE(fs, nullptr);

    // puts("Attempting to fill inode table...");

    // Dummy string to loop with
    char fname[] = "/a/a\0\0\0\0\0\0\0\0\0\0\0";  // extra space because this is all sorts of messed up now
    // If we do basic a-z, with a-z contained in each, that's... 26*15 which is ~1.5x as much as we need
    // 16 dirs of 15 fills... goes over by one. Ugh.
    // Oh man, AND we run out of space in root.
    // That's annoying.
    for (char dir = 'a'; dir < 'o'; fname[1] = ++dir) {
        fname[2] = '\0';
        ASSERT_EQ(fs_create(fs, fname, FS_DIRECTORY), 0);
        // printf("File: %s\n", fname);
        fname[2] = '/';
        for (char file = 'a'; file < 'p';) {
            fname[3] = file++;
            // printf("File: %s\n", fname);
            ASSERT_EQ(fs_create(fs, fname, FS_REGULAR), 0);
        }
    }

    // CREATE_FILE 19
    ASSERT_LT(fs_create(fs, "/a/z", FS_REGULAR), 0);
    // Catch up to finish creation
    fname[2] = '\0';
    // printf("File: %s\n", fname);
    ASSERT_EQ(fs_create(fs, fname, FS_DIRECTORY), 0);
    fname[2] = '/';
    for (char file = 'a'; file < 'o';) {
        fname[3] = file++;
        // printf("File: %s\n", fname);
        ASSERT_EQ(fs_create(fs, fname, FS_REGULAR), 0);
    }
    // ok, need to make /o/o a directory
    fname[3] = 'o';
    // printf("File: %s\n", fname);
    ASSERT_EQ(fs_create(fs, fname, FS_DIRECTORY), 0);
    // Now there's room for... Ugh, just run it till it breaks and fix it there
    // (/o/o/o apparently)
    // but THAT doesn't work because then we're at a full directory again
    // So we can't test that it failed because we're out of inodes.
    // So we have to make ANOTHER subdirectory.
    // UGGGGhhhhhhhhhhhhhhhhhhh
    fname[4] = '/';
    for (char file = 'a'; file < 'o';) {
        fname[5] = file++;
        // printf("File: %s\n", fname);
        ASSERT_EQ(fs_create(fs, fname, FS_REGULAR), 0);
    }

    fname[5] = 'o';
    // printf("File: %s\n", fname);
    ASSERT_EQ(fs_create(fs, fname, FS_DIRECTORY), 0);

    // Now. Now we are done. No more. Full table.

    // puts("Inode table full?");

    // CREATE_FILE 20
    fname[6] = '/';
    fname[7] = 'a';
    ASSERT_LT(fs_create(fs, fname, FS_REGULAR), 0);
    // save file for inspection
    fs_unmount(fs);

    // ... Can't really test 21 yet.
    score += 1;
}


/*
    int fs_open(S16FS_t *fs, const char *path)
    1. Normal, file at root
    2. Normal, file in subdir
    3. Normal, multiple fd to the same file
    4. Error, NULL fs
    5. Error, NULL fname
    6. Error, empty fname ???
    7. Error, not a regular file
    8. Error, file does not exist
    9. Error, out of descriptors

    int fs_close(S16FS_t *fs, int fd);
    1. Normal, whatever
    2. Normal, attempt to use after closing, assert failure **
    3. Normal, multiple opens, close does not affect the others **
    4. Error, FS null
    5. Error, invalid fd, positive
    6. Error, invalid fd, positive, out of bounds
    7. Error, invaid fs, negative
*/
TEST(c_tests, open_close_file) {
    vector<const char *> filenames{
        "/file", "/folder", "/folder/with_file", "/folder/with_folder", "/DOESNOTEXIST", "/file/BAD_REQUEST",
        "/DOESNOTEXIST/with_file", "/folder/with_file/bad_req", "folder/missing_slash", "/folder/new_folder/",
        "/folder/withwaytoolongfilenamethattakesupmorespacethanitshould and yet was not enough so I had to add "
        "more/bad_req",
        "/folder/withfilethatiswayyyyytoolongwhydoyoumakefilesthataretoobigEXACT!", "/", "/mystery_file"};

    const char *test_fname = "c_tests.s16fs";

    ASSERT_EQ(system("cp b_tests_normal.s16fs c_tests.s16fs"), 0);

    S16FS_t *fs = fs_mount(test_fname);

    ASSERT_NE(fs, nullptr);

    int fd_array[256] = {-1};

    // OPEN_FILE 1
    fd_array[0] = fs_open(fs, filenames[0]);
    ASSERT_GE(fd_array[0], 0);

    // CLOSE_FILE 4
    ASSERT_LT(fs_close(NULL, fd_array[0]), 0);

    // CLOSE_FILE 1
    ASSERT_EQ(fs_close(fs, fd_array[0]), 0);

    // CLOSE_FILE 2 and 3 elsewhere

    // CLOSE_FILE 5
    ASSERT_LT(fs_close(fs, 70), 0);

    // CLOSE_FILE 6
    ASSERT_LT(fs_close(fs, 7583), 0);

    // CLOSE_FILE 7
    ASSERT_LT(fs_close(fs, -18), 0);

    // OPEN_FILE 2
    fd_array[1] = fs_open(fs, filenames[2]);
    ASSERT_GE(fd_array[1], 0);

    ASSERT_EQ(fs_close(fs, fd_array[0]), 0);

    // OPEN_FILE 3
    fd_array[2] = fs_open(fs, filenames[0]);
    ASSERT_GE(fd_array[2], 0);
    fd_array[3] = fs_open(fs, filenames[0]);
    ASSERT_GE(fd_array[3], 0);
    fd_array[4] = fs_open(fs, filenames[0]);
    ASSERT_GE(fd_array[4], 0);

    ASSERT_EQ(fs_close(fs, fd_array[2]), 0);
    ASSERT_EQ(fs_close(fs, fd_array[3]), 0);
    ASSERT_EQ(fs_close(fs, fd_array[4]), 0);

    // OPEN_FILE 4
    fd_array[5] = fs_open(NULL, filenames[0]);
    ASSERT_LT(fd_array[5], 0);

    // OPEN_FILE 5
    fd_array[5] = fs_open(fs, NULL);
    ASSERT_LT(fd_array[5], 0);

    // OPEN_FILE 6
    // Uhh, bad filename? Not a slash?
    // It's wrong for a bunch of reasons, really.
    fd_array[5] = fs_open(fs, "");
    ASSERT_LT(fd_array[5], 0);

    // OPEN_FILE 7
    fd_array[5] = fs_open(fs, "/");
    ASSERT_LT(fd_array[5], 0);

    fd_array[5] = fs_open(fs, filenames[1]);
    ASSERT_LT(fd_array[5], 0);

    // OPEN_FILE 8
    fd_array[5] = fs_open(fs, filenames[6]);
    ASSERT_LT(fd_array[5], 0);

    // OPEN_FILE 9
    // In case I'm leaking descriptors, wipe them all
    fs_unmount(fs);
    fs = fs_mount(test_fname);
    ASSERT_NE(fs, nullptr);

    for (int i = 0; i < 256; ++i) {
        fd_array[i] = fs_open(fs, filenames[0]);
    }

    int err = fs_open(fs, filenames[0]);

    ASSERT_LT(err, 0);

    fs_unmount(fs);

    score += 1;
}

/*
    ssize_t fs_write(S16FS_t *fs, int fd, const void *src, size_t nbyte);
    1. Normal, 0 size to < 1 block
    2. Normal, < 1 block to next
    3. Normal, 0 size to 1 block
    4. Normal, 1 block to next
    5. Normal, 1 block to partial
    6. Normal, direct -> indirect
    7. Normal, indirect -> dbl_indirect
    8. Normal, full file (run out of blocks before max file size :/ )
    9. Error, file full/blocks full (also test fs_create 13)
    10. Error, fs NULL
    11. Error, data NULL
    12. Error, nbyte 0 (not an error...? Bad parameters? Hmm.)
    13. Error, bad fd
*/

TEST(d_tests, write_file_simple) {
    vector<const char *> fnames{"/file_a", "/file_b", "/file_c", "/file_d"};

    const char *test_fname = "d_tests_normal.s16fs";

    S16FS_t *fs = fs_format(test_fname);

    ASSERT_NE(fs, nullptr);

    uint8_t three_a[1024];
    memset(three_a, 0x33, 333);
    memset(three_a + 333, 0xAA, 691);
    // 333 0x33, rest is 0xAA
    // I really wish there was a "const but wait like 2 sec I need to write something complex"
    // (actually you can kinda do that with memset and pointer voodoo)
    uint8_t two_nine[1024];
    memset(two_nine, 0x22, 222);
    memset(two_nine + 222, 0x99, 802);
    // Figure out the pattern yet?
    uint8_t large_eight_five_b_seven[1024 * 3];
    memset(large_eight_five_b_seven, 0x88, 888);
    memset(large_eight_five_b_seven + 888, 0x55, 555);
    memset(large_eight_five_b_seven + 555 + 888, 0xBB, 1111);
    memset(large_eight_five_b_seven + 555 + 1111 + 888, 0x77, 518);

    // ... I just realized we can't read the data to verify it yet.
    // Whose idea was this, anyway.
    // I guess we'll have to manually inspect files and throw some points to that.
    // Wheeeee....
    // Or I can get the autograder to figure it out. Or make Matt do it. Probably that one.
    // Everything's automated if you make someone else do it, this is why I'm the best TA

    ASSERT_EQ(fs_create(fs, fnames[0], FS_REGULAR), 0);
    int fd_array[5] = {-1};  // wonderful arbitrary number

    fd_array[0] = fs_open(fs, fnames[0]);
    ASSERT_GE(fd_array[0], 0);

    ASSERT_EQ(fs_create(fs, fnames[1], FS_REGULAR), 0);

    fd_array[1] = fs_open(fs, fnames[1]);
    ASSERT_GE(fd_array[1], 0);

    ASSERT_EQ(fs_create(fs, fnames[2], FS_REGULAR), 0);

    fd_array[2] = fs_open(fs, fnames[2]);
    ASSERT_GE(fd_array[2], 0);

    // Alrighty, time to get some work done.
    // This FS object has one block eaten up at the moment for root, so we have...
    // 65536 - 41 = 65495 blocks. And we need to eventually use up all of them. Good.

    // FS_WRITE 1
    ASSERT_EQ(fs_write(fs, fd_array[0], three_a, 334), 334);

    // FS_WRITE 2
    ASSERT_EQ(fs_write(fs, fd_array[0], large_eight_five_b_seven, 1200), 1200);
    // file should be 333 0x33, 1 0xAA, 888 0x88 , 312 0x55 and dipping into second block

    // FS_WRITE 3
    ASSERT_EQ(fs_write(fs, fd_array[1], two_nine, 1024), 1024);

    // FS_WRITE 4
    ASSERT_EQ(fs_write(fs, fd_array[1], two_nine, 1024), 1024);
    // File has two copies of two_nine

    // FS_WRITE 5
    ASSERT_EQ(fs_write(fs, fd_array[2], large_eight_five_b_seven + 555 + 888, 1024), 1024);

    ASSERT_EQ(fs_write(fs, fd_array[2], three_a, 334), 334);
    // file is a block of 0x11, 333 0x33 and one 0xAA

    // I'll do the breakage tests now, move the big writes somewhere else
    // 2. Normal, attempt to use after closing, assert failure **
    // 3. Normal, multiple opens, close does not affect the others **

    // FS_WRITE 11
    ASSERT_LT(fs_write(NULL, fd_array[2], three_a, 999), 0);

    // FS_WRITE 12
    ASSERT_LT(fs_write(fs, fd_array[2], NULL, 999), 0);
    // Can't validate that it didn't mess up the R/W position :/

    // FS_WRITE 13
    ASSERT_EQ(fs_write(fs, fd_array[2], three_a, 0), 0);

    // FS_WRITE 14
    ASSERT_LT(fs_write(fs, 90, three_a, 12), 0);
    ASSERT_LT(fs_write(fs, -90, three_a, 12), 0);

    // FS_CLOSE 2
    ASSERT_EQ(fs_close(fs, fd_array[0]), 0);
    ASSERT_LT(fs_write(fs, fd_array[0], three_a, 500), 0);

    // FS_CLOSE 3
    fd_array[0] = fs_open(fs, fnames[1]);
    ASSERT_GE(fd_array[0], 0);
    // fd 0 and 1 point to same file

    ASSERT_EQ(fs_close(fs, fd_array[0]), 0);

    ASSERT_EQ(fs_write(fs, fd_array[1], three_a, 1024), 1024);
    // File better have two two_nines and a three_a

    // And I'm going to unmount without closing.

    fs_unmount(fs);
    score += 1;
}

TEST(d_tests, write_file_fill) {
    // Still gotta test write 6,7,8,9
    vector<const char *> fnames{"/file_a", "/file_b", "/file_c", "/file_d"};

    const char *test_fname = "d_tests_full.s16fs";

    S16FS_t *fs = fs_format(test_fname);

    ASSERT_NE(fs, nullptr);

    uint8_t large_eight_five_b_seven[1024 * 3];
    memset(large_eight_five_b_seven, 0x88, 888);
    memset(large_eight_five_b_seven + 888, 0x55, 555);
    memset(large_eight_five_b_seven + 555 + 888, 0xBB, 1111);
    memset(large_eight_five_b_seven + 555 + 1111 + 888, 0x77, 518);

    ASSERT_EQ(fs_create(fs, fnames[0], FS_REGULAR), 0);

    int fd = fs_open(fs, fnames[0]);
    ASSERT_GE(fd, 0);

    // Alrighty, time to get some work done.
    // This FS object has one block eaten up at the moment for root, so we have...
    // 65536 - 41 = 65495 blocks. And we need to eventually use up all of them. Good.

    // FS_WRITE 6
    // direct/indirect transition is easy, write 6 blocks then one more
    size_t blocks = 0;
    ASSERT_EQ(fs_write(fs, fd, large_eight_five_b_seven, 1024 * 3), 1024 * 3);
    blocks += 3;
    ASSERT_EQ(fs_write(fs, fd, large_eight_five_b_seven, 1024 * 2), 1024 * 2);
    blocks += 2;
    ASSERT_EQ(fs_write(fs, fd, large_eight_five_b_seven, 1024 * 2), 1024 * 2);
    blocks += 3;
    // 3 bceause we just ate up an indirect block


    // FS_WRITE 7
    // Ok, now we need to wriiiiittteeeeeeee......... 510?
    // Yeah. 510. 518 total blocks, 517 of data, 511 of them in indirect
    for (; blocks < 518; blocks += 2) {
        ASSERT_EQ(fs_write(fs, fd, large_eight_five_b_seven, 1024 * 2), 1024 * 2);
    }
    // Now we should be at 511 filled... write two to get into double
    ASSERT_EQ(fs_write(fs, fd, large_eight_five_b_seven, 1024 * 2), 1024 * 2);
    blocks += 4;
    // +2 because dbl indirect and the indirect... skip a paragraph

    // FS_WRITE 8
    // Ok. This one is the gross one. We have written... 6 + 512 + 1
    // There are 65495 at start, so 65495 - 519... 64976 to write.
    // UGH. Wolfram says 64976 is 2^4 * 31 * 131
    // Which is 16 iterations of a write of size 4061 blocks

    // HAHA, GOTCHA! Actually it's much more complicated and I spent an hour+ debugging before I remembered that
    // Total data (actual data) in a file is... I can never remember the equation
    // 65536 - 41 - 6 - 1 - 512 - 1 = 64975
    // 64975 = ceil(x/512) + x... x = 64848
    // 64848 = 2^4 * 3 * 7 * 193 = 16 * 4053
    // I'm 99.99999999% sure this is off by one. Or two.

    uint8_t *giant_data_hunk = new (std::nothrow) uint8_t[512 * 1024];  // ~ 4MB
    ASSERT_NE(giant_data_hunk, nullptr);
    memset(giant_data_hunk, 0x6E, 512 * 1024);
    ASSERT_EQ(fs_write(fs, fd, giant_data_hunk, 511 * 1024), 511 * 1024);
    blocks += 511;
    // exactly one indirect filled
    for (int i = 0; i < 125; ++i, blocks += 513) {
        ASSERT_EQ(fs_write(fs, fd, giant_data_hunk, 512 * 1024), 512 * 1024);
    }
    // Down to the last few blocks now
    // Gonna try and write more than is left, because you should cut it off when you get to the end, not just die.
    // According to my investigation, there's 337 blocks left
    ASSERT_EQ(fs_write(fs, fd, giant_data_hunk, 1024 * 522), 1024 * 336);
    delete[] giant_data_hunk;

    // While I'm at it...
    // FS_CREATE 21
    ASSERT_LT(fs_create(fs, fnames[1], FS_DIRECTORY), 0);

    // And might as well check this
    ASSERT_EQ(fs_create(fs, fnames[1], FS_REGULAR), 0);
    // I now realize I'm just testing my code now, since 99.99999999%
    // of you are using my P3M1 code, which just makes more work for me.
    // Good.

    // There's a handful of edge cases that these tests won't catch.
    // But I tried, so nobody can judge me.


    fs_unmount(fs);

    score += 1;
}

/*
    int fs_remove(S16FS_t *fs, const char *path);
    1. Normal, file at root
    2. Normal, file in subdirectory (attept to write to already opened descriptor to this afterwards - DESCRIPTOR ONLY)
    3. Normal, directory in subdir, empty directory
    4. Normal, file in double indirects somewhere (use full fs file from write_file?)
    5. Error, directory with contents
    6. Error, file does not exist
    7. Error, Root (also empty)
    8. Error, NULL fs
    9. Error, NULL fname
    10. Error, Empty fname (same as file does not exist?)
*/
TEST(e_tests, remove_file) {
    vector<const char *> b_fnames{
        "/file", "/folder", "/folder/with_file", "/folder/with_folder", "/DOESNOTEXIST", "/file/BAD_REQUEST",
        "/DOESNOTEXIST/with_file", "/folder/with_file/bad_req", "folder/missing_slash", "/folder/new_folder/",
        "/folder/withwaytoolongfilenamethattakesupmorespacethanitshould and yet was not enough so I had to add "
        "more/bad_req",
        "/folder/withfilethatiswayyyyytoolongwhydoyoumakefilesthataretoobigEXACT!", "/", "/mystery_file"};

    vector<const char *> a_fnames{"/file_a", "/file_b", "/file_c", "/file_d"};

    const char *(test_fname[2]) = {"e_tests_a.s16fs", "e_tests_b.s16fs"};

    ASSERT_EQ(system("cp d_tests_full.s16fs e_tests_a.s16fs"), 0);
    ASSERT_EQ(system("cp c_tests.s16fs e_tests_b.s16fs"), 0);

    S16FS_t *fs = fs_mount(test_fname[1]);
    ASSERT_NE(fs, nullptr);

    // FS_REMOVE 10
    ASSERT_LT(fs_remove(fs, ""), 0);

    // FS_REMOVE 2
    int fd = fs_open(fs, b_fnames[2]);
    ASSERT_GE(fd, 0);

    ASSERT_EQ(fs_remove(fs, b_fnames[2]), 0);

    ASSERT_LT(fs_write(fs, fd, b_fnames[0], 6), 0);

    // FS_REMOVE 5
    ASSERT_LT(fs_remove(fs, b_fnames[1]), 0);


    ASSERT_EQ(fs_remove(fs, b_fnames[3]), 0);

    // FS_REMOVE 3
    ASSERT_EQ(fs_remove(fs, b_fnames[1]), 0);


    fs_unmount(fs);


    fs = fs_mount(test_fname[0]);
    ASSERT_NE(fs, nullptr);

    // FS_REMOVE 1
    ASSERT_EQ(fs_remove(fs, a_fnames[1]), 0);

    // FS_REMOVE 4
    ASSERT_EQ(fs_remove(fs, a_fnames[0]), 0);

    // FS_REMOVE 6
    ASSERT_LT(fs_remove(fs, a_fnames[3]), 0);

    // FS_REMOVE 7
    ASSERT_LT(fs_remove(fs, "/"), 0);

    // FS_REMOVE 8
    ASSERT_LT(fs_remove(NULL, a_fnames[1]), 0);

    // FS_REMOVE 9
    ASSERT_LT(fs_remove(fs, NULL), 0);

    fs_unmount(fs);

    score += 1;
}

/*
    int fs_get_dir(const S16FS_t *const fs, const char *const fname, dir_rec_t *const records)
    1. Normal, root I guess?
    2. Normal, subdir somewhere
    3. Normal, empty dir
    4. Error, bad path
    5. Error, NULL fname
    6. Error, NULL fs
    7. Error, not a directory
*/
TEST(f_tests, get_dir) {
    vector<const char *> fnames{
        "/file", "/folder", "/folder/with_file", "/folder/with_folder", "/DOESNOTEXIST", "/file/BAD_REQUEST",
        "/DOESNOTEXIST/with_file", "/folder/with_file/bad_req", "folder/missing_slash", "/folder/new_folder/",
        "/folder/withwaytoolongfilenamethattakesupmorespacethanitshould and yet was not enough so I had to add "
        "more/bad_req",
        "/folder/withfilethatiswayyyyytoolongwhydoyoumakefilesthataretoobigEXACT!", "/", "/mystery_file"};

    const char *test_fname = "f_tests.s16fs";

    ASSERT_EQ(system("cp c_tests.s16fs f_tests.s16fs"), 0);

    S16FS_t *fs = fs_mount(test_fname);
    ASSERT_NE(fs, nullptr);

    // FS_GET_DIR 1
    dyn_array_t *record_results = fs_get_dir(fs, "/");
    ASSERT_NE(record_results, nullptr);

    ASSERT_TRUE(find_in_directory(record_results, "file"));
    ASSERT_TRUE(find_in_directory(record_results, "folder"));
    ASSERT_EQ(dyn_array_size(record_results), 2);

    dyn_array_destroy(record_results);

    // FS_GET_DIR 2
    record_results = fs_get_dir(fs, fnames[1]);
    ASSERT_NE(record_results, nullptr);

    ASSERT_TRUE(find_in_directory(record_results, "with_file"));
    ASSERT_TRUE(find_in_directory(record_results, "with_folder"));
    ASSERT_EQ(dyn_array_size(record_results), 2);

    dyn_array_destroy(record_results);

    // FS_GET_DIR 3
    record_results = fs_get_dir(fs, fnames[3]);
    ASSERT_NE(record_results, nullptr);

    ASSERT_EQ(dyn_array_size(record_results), 0);

    dyn_array_destroy(record_results);

    // FS_GET_DIR 4
    record_results = fs_get_dir(fs, fnames[9]);
    ASSERT_EQ(record_results, nullptr);

    // FS_GET_DIR 5
    record_results = fs_get_dir(fs, NULL);
    ASSERT_EQ(record_results, nullptr);

    // FS_GET_DIR 6
    record_results = fs_get_dir(NULL, fnames[3]);
    ASSERT_EQ(record_results, nullptr);

    // FS_GET_DIR 7
    record_results = fs_get_dir(fs, fnames[0]);
    ASSERT_EQ(record_results, nullptr);

    fs_unmount(fs);

    score += 25;
}

/*
    off_t fs_seek(S16FS_t *fs, int fd, off_t offset, seek_t whence)
    1. Normal, wherever, really - make sure it doesn't change a second fd to the file
    2. Normal, seek past beginning - resulting location unspecified by our api, can't really test?
    3. Normal, seek past end - resulting location unspecified by our api, can't really test?
    4. Error, FS null
    5. Error, fd invalid
    6. Error, whence not a valid value
*/

TEST(g_tests, seek) {
    vector<const char *> fnames{"/file_a", "/file_b", "/file_c", "/file_d"};

    const char *test_fname = "g_tests.s16fs";

    ASSERT_EQ(system("cp d_tests_full.s16fs g_tests.s16fs"), 0);

    S16FS_t *fs = fs_mount(test_fname);
    ASSERT_NE(fs, nullptr);

    int fd_one = fs_open(fs, fnames[0]);
    ASSERT_GE(fd_one, 0);

    int fd_two = fs_open(fs, fnames[0]);
    ASSERT_GE(fd_two, 0);

    // While we're at it, make sure they default to 0

    int position = fs_seek(fs, fd_one, 0, FS_SEEK_CUR);
    ASSERT_EQ(position, 0);

    // FS_SEEK 1
    position = fs_seek(fs, fd_one, 1023, FS_SEEK_CUR);
    ASSERT_EQ(position, 1023);

    position = fs_seek(fs, fd_one, 12, FS_SEEK_SET);
    ASSERT_EQ(position, 12);

    // FS_SEEK 2
    position = fs_seek(fs, fd_one, -50, FS_SEEK_CUR);
    ASSERT_EQ(position, 0);

    // FS_SEEK 3
    position = fs_seek(fs, fd_one, 98675309, FS_SEEK_CUR);
    ASSERT_EQ(position, 66934784);

    // while we're at it, make sure seek didn't break the other one
    position = fs_seek(fs, fd_two, 0, FS_SEEK_CUR);
    ASSERT_EQ(position, 0);

    // FS_SEEK 4
    position = fs_seek(NULL, fd_one, 12, FS_SEEK_SET);
    ASSERT_LT(position, 0);

    // FS_SEEK 5
    position = fs_seek(fs, 98, 12, FS_SEEK_SET);
    ASSERT_LT(position, 0);

    // FS_SEEK 6
    position = fs_seek(fs, fd_one, 12, (seek_t) 8458);
    ASSERT_LT(position, 0);

    fs_unmount(fs);

    score += 13;
}

/*
    ssize_t fs_read(S16FS_t *fs, int fd, void *dst, size_t nbyte);
    1. Normal, begin to < 1 block
    2. Normal, < 1 block to part of next
    3. Normal, whole block
    4. Normal, multiple blocks
    5. Normal, direct->indirect transition
    6. Normal, indirect->dbl_indirect transition
    7. Normal, double indirect indirect transition
    8. Error, NULL fs
    9. Error, NULL data
    10. Normal, nbyte 0
    11. Normal, at EOF
*/

TEST(h_tests, read) {
    vector<const char *> fnames{"/file_a", "/file_b", "/file_c", "/file_d"};

    const char *test_fname = "g_tests.s16fs";

    ASSERT_EQ(system("cp d_tests_full.s16fs g_tests.s16fs"), 0);

    uint8_t six_e[3072];
    memset(six_e, 0x6E, 3072);
    uint8_t large_eight_five_b_seven[1024 * 3];
    memset(large_eight_five_b_seven, 0x88, 888);
    memset(large_eight_five_b_seven + 888, 0x55, 555);
    memset(large_eight_five_b_seven + 555 + 888, 0xBB, 1111);
    memset(large_eight_five_b_seven + 555 + 1111 + 888, 0x77, 518);

    S16FS_t *fs = fs_mount(test_fname);
    ASSERT_NE(fs, nullptr);

    int fd = fs_open(fs, fnames[0]);
    ASSERT_GE(fd, 0);

    uint8_t write_space[4096] = {0};

    // FS_READ 1
    ssize_t nbyte = fs_read(fs, fd, &write_space, 889);
    ASSERT_EQ(nbyte, 889);
    ASSERT_EQ(memcmp(write_space, large_eight_five_b_seven, 889), 0);

    // FS_READ 2
    nbyte = fs_read(fs, fd, &write_space, 560);
    ASSERT_EQ(nbyte, 560);
    ASSERT_EQ(memcmp(write_space, large_eight_five_b_seven + 889, 560), 0);

    // FS_READ 3
    ASSERT_EQ(fs_seek(fs, fd, 0, FS_SEEK_SET), 0);
    nbyte = fs_read(fs, fd, &write_space, 1024);
    ASSERT_EQ(nbyte, 1024);
    ASSERT_EQ(memcmp(write_space, large_eight_five_b_seven, 1024), 0);

    // FS_READ 4
    nbyte = fs_read(fs, fd, &write_space, 2048);
    ASSERT_EQ(nbyte, 2048);
    ASSERT_EQ(memcmp(write_space, large_eight_five_b_seven + 1024, 2048), 0);

    // FS_READ 5
    ASSERT_EQ(fs_seek(fs, fd, 5120, FS_SEEK_SET), 5120);
    nbyte = fs_read(fs, fd, &write_space, 2048);
    ASSERT_EQ(nbyte, 2048);
    ASSERT_EQ(memcmp(write_space, large_eight_five_b_seven, 2048), 0);
    // Aww yeah, my read hasn't broken yet while writing tests

    // FS_READ 6
    ASSERT_EQ(fs_seek(fs, fd, (511 + 6) * 1024, FS_SEEK_SET), (511 + 6) * 1024);
    nbyte = fs_read(fs, fd, &write_space, 2048);
    ASSERT_EQ(nbyte, 2048);
    // Spoke too soon
    // Duh, checked against the wrong result data.
    ASSERT_EQ(memcmp(write_space, large_eight_five_b_seven, 2048), 0);

    score += 15;

    // FS_READ 7
    ASSERT_EQ(fs_seek(fs, fd, (512 * 10 + 511 + 6) * 1024, FS_SEEK_SET), (512 * 10 + 511 + 6) * 1024);
    nbyte = fs_read(fs, fd, &write_space, 2048);
    ASSERT_EQ(nbyte, 2048);
    ASSERT_EQ(memcmp(write_space, six_e, 2048), 0);

    // FS_READ 8
    nbyte = fs_read(NULL, fd, write_space, 1024);
    ASSERT_LT(nbyte, 0);
    // did you break the descriptor position?
    ASSERT_EQ(fs_seek(fs, fd, 0, FS_SEEK_CUR), (512 * 10 + 511 + 6) * 1024 + 2048);

    // FS_READ 9
    nbyte = fs_read(fs, fd, NULL, 1024);
    ASSERT_LT(nbyte, 0);
    // did you break the descriptor position?
    ASSERT_EQ(fs_seek(fs, fd, 0, FS_SEEK_CUR), (512 * 10 + 511 + 6) * 1024 + 2048);

    // FS_READ 10
    nbyte = fs_read(fs, fd, write_space, 0);
    ASSERT_EQ(nbyte, 0);
    ASSERT_EQ(fs_seek(fs, fd, 0, FS_SEEK_CUR), (512 * 10 + 511 + 6) * 1024 + 2048);

    // FS_READ 11
    ASSERT_EQ(fs_seek(fs, fd, 98675309, FS_SEEK_CUR), 66934784);
    ASSERT_EQ(fs_seek(fs, fd, -500, FS_SEEK_END), 66934284);
    nbyte = fs_read(fs, fd, write_space, 1024);
    ASSERT_EQ(nbyte, 500);
    ASSERT_EQ(memcmp(write_space, six_e, 500), 0);
    // did you mess up the position?
    ASSERT_EQ(fs_seek(fs, fd, 0, FS_SEEK_CUR), 66934784);

    fs_unmount(fs);

    score += 20;
}

// 80 points left, 10 for valgrind

#ifdef ENABLE_MOVE


/*
    int fs_move(S16FS_t *fs, const char *src, const char *dst);
    1. Normal, file, one dir to another (check descriptor)
    2. Normal, directory
    3. Normal, Rename of file where the directory is full
    4. Error, dst exists
    5. Error, dst parent does not exist
    6. Error, dst parent full
    7. Error, src does not exist
    8. ?????, src = dst
    9. Error, FS null
    10. Error, src null
    11. Error, src is root
    12. Error, dst NULL
    13. Error, dst root?
    14. Error, Directory into itself
*/

TEST(i_tests, move) {
    vector<const char *> fnames{
        "/file", "/folder", "/folder/with_file", "/folder/with_folder", "/DOESNOTEXIST", "/file/BAD_REQUEST",
        "/DOESNOTEXIST/with_file", "/folder/with_file/bad_req", "folder/missing_slash", "/folder/new_folder/",
        "/folder/withwaytoolongfilenamethattakesupmorespacethanitshould and yet was not enough so I had to add "
        "more/bad_req",
        "/folder/withfilethatiswayyyyytoolongwhydoyoumakefilesthataretoobigEXACT!", "/", "/mystery_file"};

    const char *test_fname = "g_tests.s16fs";

    ASSERT_EQ(system("cp c_tests.s16fs g_tests.s16fs"), 0);

    S16FS_t *fs = fs_mount(test_fname);

    ASSERT_NE(fs, nullptr);

    int fd = fs_open(fs, fnames[0]);

    // FS_MOVE 1
    dyn_array_t *record_results = NULL;
    ASSERT_EQ(fs_move(fs, fnames[0], "/folder/new_location"), 0);

    record_results = fs_get_dir(fs, fnames[1]);
    ASSERT_NE(record_results, nullptr);

    ASSERT_TRUE(find_in_directory(record_results, "with_file"));
    ASSERT_TRUE(find_in_directory(record_results, "with_folder"));
    ASSERT_TRUE(find_in_directory(record_results, "new_location"));
    ASSERT_EQ(dyn_array_size(record_results), 3);
    dyn_array_destroy(record_results);

    record_results = fs_get_dir(fs, "/");
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "folder"));
    ASSERT_EQ(dyn_array_size(record_results), 1);
    dyn_array_destroy(record_results);


    // Descriptor still functional?
    ASSERT_EQ(fs_write(fs, fd, test_fname, 14), 14);

    // FS_MOVE 2
    ASSERT_EQ(fs_move(fs, fnames[3], "/with_folder"), 0);
    record_results = fs_get_dir(fs, "/");
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "folder"));
    ASSERT_TRUE(find_in_directory(record_results, "with_folder"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    record_results = fs_get_dir(fs, fnames[1]);
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "with_file"));
    ASSERT_TRUE(find_in_directory(record_results, "new_location"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    // FS_MOVE 4
    ASSERT_LT(fs_move(fs, "/folder/new_location", fnames[1]), 0);

    record_results = fs_get_dir(fs, "/");
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "folder"));
    ASSERT_TRUE(find_in_directory(record_results, "with_folder"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    record_results = fs_get_dir(fs, fnames[1]);
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "with_file"));
    ASSERT_TRUE(find_in_directory(record_results, "new_location"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    // FS_MOVE 5
    ASSERT_LT(fs_move(fs, "/folder/new_location", "/folder/noooope/new_location"), 0);

    record_results = fs_get_dir(fs, "/");
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "folder"));
    ASSERT_TRUE(find_in_directory(record_results, "with_folder"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    record_results = fs_get_dir(fs, fnames[1]);
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "with_file"));
    ASSERT_TRUE(find_in_directory(record_results, "new_location"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    // FS_MOVE 7
    ASSERT_LT(fs_move(fs, "/folder/DNE", "/folder/also_DNE"), 0);

    record_results = fs_get_dir(fs, "/");
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "folder"));
    ASSERT_TRUE(find_in_directory(record_results, "with_folder"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    record_results = fs_get_dir(fs, fnames[1]);
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "with_file"));
    ASSERT_TRUE(find_in_directory(record_results, "new_location"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    // FS_MOVE 8
    // this one is just weird, so... skipping

    // FS_MOVE 9
    ASSERT_LT(fs_move(NULL, "/folder/DNE", "/folder/also_DNE"), 0);

    // FS_MOVE 10
    ASSERT_LT(fs_move(fs, NULL, "/folder/also_DNE"), 0);

    // FS_MOVE 11
    ASSERT_LT(fs_move(fs, "/", "/folder/root_maybe"), 0);

    // FS_MOVE 12
    ASSERT_LT(fs_move(fs, "/folder/new_location", NULL), 0);

    // FS_MOVE 13
    ASSERT_LT(fs_move(fs, "/folder/new_location", "/"), 0);

    // FS_MOVE 14
    ASSERT_LT(fs_move(fs,"/folder","/folder/oh_no"),0);


    // Things still working after all that ?
    record_results = fs_get_dir(fs, "/");
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "folder"));
    ASSERT_TRUE(find_in_directory(record_results, "with_folder"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    record_results = fs_get_dir(fs, fnames[1]);
    ASSERT_NE(record_results, nullptr);
    ASSERT_TRUE(find_in_directory(record_results, "with_file"));
    ASSERT_TRUE(find_in_directory(record_results, "new_location"));
    ASSERT_EQ(dyn_array_size(record_results), 2);
    dyn_array_destroy(record_results);

    fs_unmount(fs);

    score += 15;
}
#endif

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GradeEnvironment);
    return RUN_ALL_TESTS();
}
