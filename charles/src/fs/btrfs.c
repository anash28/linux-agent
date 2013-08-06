/**
	File:        btrfs.c
	Author:      Charles Roydhouse
	             Nick Garvey
	Description: btrfs filesystem parsing functions
*/

#include "btrfs.h"
#include "fs.h"

int btrfs_has_identifier(int fd) {
	return 0;
}

int btrfs_iter_blocks(const char *dev, int (*callback)(int fd, uint64_t length, uint64_t offset)) {
	return 0;
}
