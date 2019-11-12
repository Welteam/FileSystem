/********************************************************/
/*                                                      */
/* Command: ./futofs [fuse args/mountpoint] [dump file] */
/* Example: ./futofs -d /tmp/futosfsHG test_tosfs_files */
/*                                                      */
/********************************************************/

#define FUSE_USE_VERSION 26

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "tosfs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

// Made it global to access memory from all functions
void *mmappedData;

// Fill stat infos on file
static int fu_stat(fuse_ino_t ino, struct stat *stbuf)
{
	stbuf->st_ino = ino;
	struct tosfs_superblock *superBlock = mmappedData;
	/* In a complete FS, the bitmap would be used to determine if the requested inode is empty or not.
	Here, since files can't be deleted, the existing inodes are always at the beginning.*/
	if (ino <= superBlock->inodes){
		struct tosfs_inode *inode = mmappedData + superBlock->block_size + ino*sizeof(struct tosfs_inode);
		stbuf->st_mode = inode->mode;
		stbuf->st_nlink = inode->nlink;
		stbuf->st_uid = inode->uid;
		stbuf->st_gid = inode->gid;
		stbuf->st_size = inode->size;
		stbuf->st_blksize = superBlock->block_size;
		stbuf->st_blocks = superBlock->blocks;
		stbuf->st_atime = 2272147200; // Last access date should be 2042-01-01.
	} else {
		return -1;
	}
	return 0;
}


static void fu_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct stat stbuf; //Create return value

	memset(&stbuf, 0, sizeof(stbuf)); //fill stbuf with 0
	if (fu_stat(ino, &stbuf) == -1)
		fuse_reply_err(req, ENOENT);
	else
		fuse_reply_attr(req, &stbuf, 1.0);
}

// Contain list of all files in directory
struct dirbuf {
	char *p;
	size_t size;
};

// Add a file to dirbuff
static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name, fuse_ino_t ino)
{
	struct stat stbuf;
	size_t oldsize = b->size;
	// Set the size for the list of files
	b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	// Allocate memory for the list
	b->p = (char *) realloc(b->p, b->size);
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	// Add entry to buffer
	fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf, b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

// Return list of files in root directory
static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize, off_t off, size_t maxsize)
{
	if (off < bufsize)
		return fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize));
	else
		return fuse_reply_buf(req, NULL, 0);
}

static void fu_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	if (ino != 1) // The only directory is root
		fuse_reply_err(req, ENOTDIR);
	else {
		struct dirbuf b;
		struct tosfs_superblock *superBlock = mmappedData;
		struct tosfs_inode *inode = mmappedData + superBlock->block_size;
		struct tosfs_dentry *entries = mmappedData + 2 * superBlock->block_size;

		memset(&b, 0, sizeof(b));
		int i;
		// Add all files to buffer
		for (i = 0; i < superBlock->inodes + 1; i++){
		    // For the last argument, 1 is a file and 2 is a directory which is equivalent to the result of this test
			dirbuf_add(req, &b, entries->name, (inode->mode == 33188) + 1);
			entries++;
			inode++;
		}
		reply_buf_limited(req, b.p, b.size, off, size);
		free(b.p); // Free memory used for the buffer
	}
}


static void fu_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param e;
	char name_check = 0;
	struct tosfs_superblock *superBlock = mmappedData;
	struct tosfs_dentry *entries = mmappedData + 2*superBlock->block_size;
	void *end = entries + superBlock->inodes + 1;
	memset(&e, 0, sizeof(e));
	// Search inode value for file. Return an error if two files have the same name
	while ((void *)entries < end){
		if (strcmp(name, entries->name) == 0){
			name_check += 1;
			e.ino = entries->inode;
		}
		entries++;
	}

	if (parent != 1 || name_check != 1)
		fuse_reply_err(req, ENOENT);
	else {
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
		fu_stat(e.ino, &e.attr);

		fuse_reply_entry(req, &e);
	}
}


static void fu_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	(void) fi;
	struct tosfs_superblock *superBlock = mmappedData;
	struct tosfs_inode *inode = mmappedData + superBlock->block_size + ino*sizeof(struct tosfs_inode);

	assert(inode->mode == 33188); //Assert if it's a file
	char *text = mmappedData + (ino + 1)*superBlock->block_size; // Point to memory block for the file
	reply_buf_limited(req, text, strlen(text), off, size);
}


static void fu_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	(void) fi;
	struct tosfs_superblock *superBlock = mmappedData;
	if ((strlen(name) <= 32) & (parent == 1) & (superBlock->inodes < superBlock->blocks)){
		/* Since files can't be deleted and are stored in order of creation, the number of file is enough to find where to store the data and inode.
		For example, the third file will be stored in the fifth block and referenced in the fifth inode.
		An improved FS would use the bitmaps.*/
		int fileNumber = superBlock->inodes;
		// Modify bitmaps
		superBlock->block_bitmap = (superBlock->block_bitmap<<1) + 1;
		superBlock->inode_bitmap = (superBlock->inode_bitmap<<1) + 2;
		superBlock->inodes += 1;

		// Fill inode infos
		struct tosfs_inode *inode = mmappedData + superBlock->block_size + (fileNumber + 1) * sizeof(struct tosfs_inode);
		inode->inode = fileNumber + 1;
		inode->block_no = fileNumber + 1;
		inode->mode = mode;
		inode->perm = 438;
		inode->nlink = 1;

		// Fill entry infos
		struct tosfs_dentry *entry = mmappedData + 2 * superBlock->block_size + (fileNumber + 1) * sizeof(struct tosfs_dentry);
		entry->inode = fileNumber + 1;
		strcpy(entry->name,  name);

		// Fill return value
		struct fuse_entry_param e;
		e.ino = inode->inode;
		fu_stat(e.ino, &e.attr);
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
		fuse_reply_create(req, &e, fi);
	} else if (superBlock->inodes == superBlock->blocks) {
		fuse_reply_err(req, ENOSPC); // Disk is full
	} else {
		fuse_reply_err(req, E2BIG); // Name too long
	}
}


static void fu_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	(void) fi;
	struct tosfs_superblock *superBlock = mmappedData;
	if (off + size < superBlock->block_size) {
		struct tosfs_inode *inode = mmappedData + superBlock->block_size + ino*sizeof(struct tosfs_inode);
		char *writePosition = mmappedData + (inode->block_no + 1)* superBlock->block_size + off;
		inode->size = off + size;
		printf("Size: %d\n", inode->size);
		strcpy(writePosition, buf);
		fuse_reply_write(req, size);
	} else {
		fuse_reply_err(req, EFBIG); // File size would be superior to block size
	}
}


static struct fuse_lowlevel_ops file_oper = {
	.lookup		= fu_lookup,
	.getattr	= fu_getattr,
	.readdir	= fu_readdir,
	//.open		= fu_open,
	.read		= fu_read,
	.create		= fu_create,
	.write		= fu_write,
};


int main(int argc, char** argv)
{
	int fd = open(argv[argc-1], O_RDWR); //Open dump of file system
	if (fd == -1) {
		perror("file");
	}
	if ((argc >= 3) & (fd != -1)){
		struct stat *buf = malloc(sizeof(struct stat)); //To store stats on target file

		//Use stat function to get size of memory dump
		stat(argv[argc-1], buf);
		off_t size = buf->st_size;

		mmappedData = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
		if (mmappedData == (void*) -1){
			perror("mmap");
			return -1;
		}
		
		// Print all informations on mapping
		printf("Address of mapping: %p\n", mmappedData);
		struct tosfs_superblock *superBlock = mmappedData;
		printf("Superblock:\nMagic number: %#x\n", superBlock->magic);
		printf("Bitmap: %d\n", superBlock->block_bitmap);
		printf("Inode bitmap: %d\n", superBlock->inode_bitmap);
		printf("Block size: %d\n", superBlock->block_size);
		printf("Number of blocks: %d\n", superBlock->blocks);
		printf("Number of inodes: %d\n", superBlock->inodes);
		printf("Inode root: %d\n\n", superBlock->root_inode);
		struct tosfs_inode *inodes = mmappedData + superBlock->block_size;
		struct tosfs_inode *p = inodes+1;
		while (p <= inodes + superBlock->inodes){
			printf("Inode %d :\n", p->inode);
			printf("Block number: %d\n", p->block_no);
			printf("UID: %d\n", p->uid);
			printf("GID: %d\n", p->gid);
			printf("Mode: %d\n", p->mode);
			printf("Permissions: %d\n", p->perm);
			printf("Size: %d\n", p->size);
			printf("Number of links: %d\n\n", p->nlink);
			p++;
		}

		struct tosfs_dentry *entries = mmappedData + 2*superBlock->block_size;
		struct tosfs_dentry *p2 = entries;
		while (p2 < entries + superBlock->inodes + 1){
			printf("Inode %d: ", p2->inode);
			printf("%s\n", p2->name);
			p2++;
		}
		printf("\n");

		char *text = mmappedData + 3*superBlock->block_size;
		printf("one_file: \"%s\"\n", text);
		text = text + superBlock->block_size;
		printf("two_file: \"%s\"\n\n", text);

        // Set up FS with FUSE library
		struct fuse_args args = *(struct fuse_args *)malloc(sizeof(struct fuse_args));
		args.argc = argc-1;
		args.argv = argv;
		args.allocated = 0;
		struct fuse_chan *ch;
		char *mountpoint;
		int err = -1;

		if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
		    (ch = fuse_mount(mountpoint, &args)) != NULL) {
			struct fuse_session *se;

			se = fuse_lowlevel_new(&args, &file_oper,
					       sizeof(file_oper), NULL);
			if (se != NULL) {
				if (fuse_set_signal_handlers(se) != -1) {
					fuse_session_add_chan(se, ch);
					err = fuse_session_loop(se);
					fuse_remove_signal_handlers(se);
					fuse_session_remove_chan(ch);
				}
				fuse_session_destroy(se);
			}
			fuse_unmount(mountpoint, ch);
		}
		fuse_opt_free_args(&args);
		munmap(mmappedData, size);

		return err ? 1 : 0;
	} else {
		printf("Arguments must be: futofs [fuse args/mountpoint] [dump file]\n"); //Instructions
		return 0;
	}
}
