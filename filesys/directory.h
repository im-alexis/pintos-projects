#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>

#include "devices/block.h"
#include "filesys/inode.h"

/* Maximum length of a file name component.
 * This is the traditional UNIX maximum length.
 * After directories are implemented, this maximum length may be
 * retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

/* A directory. */
struct dir
{
    struct inode *inode; /* Backing store. */
    off_t pos;           /* Current position. */
};
/*
        WE COULD ADD THIS TO QUICKLY GET relative path and and parent
        FROM THE VIDEO


 *           +---------------++---------------+
 *           |      NAME     |      INODE     |
 *           +---------------++---------------+
 *           |      "."      |     CURRENT    |
 *           +---------------++---------------+
 *           |      ".."     |      PARENT    |
 *           +---------------++---------------+
 *           |      ...      |      OTHER     |
 *           +---------------++---------------+
 *

*/
/* A single directory entry. */
struct dir_entry
{
    block_sector_t inode_sector; /* Sector number of header. */
    char name[NAME_MAX + 1];     /* Null terminated file name. */
    bool in_use;                 /* In use or free? */
};

/* Opening and closing directories. */
bool dir_create(block_sector_t sector, size_t entry_cnt);
struct dir *dir_open(struct inode *);
struct dir *dir_open_root(void);
struct dir *dir_reopen(struct dir *);
void dir_close(struct dir *);
struct inode *dir_get_inode(struct dir *);

/* Reading and writing. */
bool dir_lookup(const struct dir *, const char *name, struct inode **);
bool dir_add(struct dir *, const char *name, block_sector_t);
bool dir_remove(struct dir *, const char *name);
bool dir_readdir(struct dir *, char name[NAME_MAX + 1]);

/* NEW FUNCTIONS*/
bool is_root_dir(struct dir *dir);
#endif /* filesys/directory.h */
