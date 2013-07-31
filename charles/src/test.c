/**
	File:        test.c
	Author:      Charles Roydhouse
	Description: Tests functionality
*/

#include <stdio.h>
#include "fsparser.h"
#include "fs/fs.h"
#include <unistd.h>

#if !(defined TRUE && defined FALSE)
	#define TRUE 1
	#define FALSE 0
#endif

#define _TO_STR(n) #n
#define _BOOL_TO_STR(b) (b == 0 ? "FAIL" : "PASS")
#define _TEST_BASE(desc, func, value, comp) printf("[%s] Testing %s\n", _BOOL_TO_STR(func comp value), desc);
#define TEST_EQ(desc, func, value) _TEST_BASE(desc, func, value, ==)
#define TEST_NE(desc, func, value) _TEST_BASE(desc, func, value, !=)
#define TEST_GT(desc, func, value) _TEST_BASE(desc, func, value, >)
#define TEST_GTE(desc, func, value) _TEST_BASE(desc, func, value, >=)
#define TEST_LT(desc, func, value) _TEST_BASE(desc, func, value, <)
#define TEST_LTE(desc, func, value) _TEST_BASE(desc, func, value, <=)

#define XFS_TEST_PARTITION "/dev/sdb1"
#define REISER_TEST_PARTITION "/dev/sdb2"
#define EXT2_TEST_PARTITION "/dev/sdb3"

int test_callback(const int fd, const uint64_t length, uint64_t offset) {
	//printf("Block length %ld at 0x%lx\n", length, offset);
	return 0;
}

void test_xfs(void) {
	TEST_EQ("XFS identifcation", fs_identify(XFS_TEST_PARTITION, FS_XFS_T), TRUE)
	TEST_EQ("XFS misidentifcation", fs_identify(REISER_TEST_PARTITION, FS_XFS_T), FALSE)
	TEST_EQ("XFS misidentifcation", fs_identify(EXT2_TEST_PARTITION, FS_XFS_T), FALSE)
	TEST_EQ("XFS copied blocks", fs_iter_blocks(XFS_TEST_PARTITION, FS_XFS_T, &test_callback), 5079040)
}

void test_reiserfs(void) {
	TEST_EQ("ReiserFS identifcation", fs_identify(REISER_TEST_PARTITION, FS_REISERFS_T), TRUE)
	TEST_EQ("ReiserFS misidentifcation", fs_identify(XFS_TEST_PARTITION, FS_REISERFS_T), FALSE)
	TEST_EQ("ReiserFS misidentifcation", fs_identify(EXT2_TEST_PARTITION, FS_REISERFS_T), FALSE)
	TEST_EQ("ReiserFS copied blocks", fs_iter_blocks(REISER_TEST_PARTITION, FS_REISERFS_T, &test_callback), 8219)
}

void test_ext2(void) {
	TEST_EQ("ext2 identifcation", fs_identify(EXT2_TEST_PARTITION, FS_EXT2_T), TRUE)
	TEST_EQ("ext2 misidentifcation", fs_identify(REISER_TEST_PARTITION, FS_EXT2_T), FALSE)
	TEST_EQ("ext2 misidentifcation", fs_identify(XFS_TEST_PARTITION, FS_EXT2_T), FALSE)
	TEST_EQ("ext2 copied blocks", fs_iter_blocks(EXT2_TEST_PARTITION, FS_EXT2_T, &test_callback), 4159)
}

int main(void) {
	test_xfs();
	test_reiserfs();
	test_ext2();
	return 0;
}
