/*
 * tosfs/tosfs.h
 * 
 * 
 * (c) 2012 David Picard - picard@ensea.fr
 * 
 */


#ifndef __TOSFS__
#define __TOSFS__

#include <linux/types.h>

#define TOSFS_MAGIC 0x1b19b10c
#define TOSFS_BLOCK_SIZE 4096
#define TOSFS_SUPERBLOCK 0
#define TOSFS_INODE_BLOCK 1
#define TOSFS_ROOT_INODE 1
#define TOSFS_ROOT_BLOCK 2
#define TOSFS_MAX_NAME_LENGTH 32
#define TOSFS_INODE_SIZE sizeof(struct tosfs_inode)

#define tosfs_set_bit(bitmap, block_no) bitmap|=(1<<block_no);

/* superblock on disk */
struct tosfs_superblock {
	__u32 magic; /* magic number */
	__u32 block_bitmap; /* bitmap for block (32 blocks) */
	__u32 inode_bitmap; /* bitmap for inode (32 inodes) */
	__u32 block_size; /* set to 4096 bytes */
	__u32 blocks; /* number of blocks, set to 32 */
	__u32 inodes; /* number of ino, max = 32 */
	__u32 root_inode; /* root inode inode */
};

/* on disk inode */
struct tosfs_inode {
	__u32 inode; /* inode number */
	__u32 block_no; /* block number for data. 1 block per file. 
					   should be inode+TOSFS_INODE_BLOCK */
	__u16 uid; /* user id */
	__u16 gid; /* group id */
	__u16 mode; /* mode (fil, dir, etc) */
	__u16 perm; /* permissions */
	__u16 size; /* size in byte (max 1 block) */
	__u16 nlink; /* link (number of hardlink) */
};

/* dentry struct on disk */
struct tosfs_dentry {
	__u32 inode; /* inode number */
	char name[TOSFS_MAX_NAME_LENGTH]; /* name of file */
};

/* inode cache */
//yypstruct tosfs_inode inode_cache[32*TOSFS_INODE_SIZE];
struct tosfs_inode *inode_cache;


/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- end macros --- */

#endif /* __TOSFS__ */

