#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <list.h>
#include <stdbool.h>

#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/thread.h"

#define NUMBER_DIRECT_BLOCKS 123
#define NUMBER_INDIRECT_BLOCKS_PER_SECTOR 128

struct bitmap;
/* On-disk inode.
 * Must be exactly BLOCK_SECTOR_SIZE (512 Bytes) bytes long.
 & FOR A MAX OF 8MB file need 16384 sectors
 */
struct inode_disk
{
    /*
    ! Remove start, when ready, increase 122 to 123 in the direct_map_table
    */
    block_sector_t start; /* First data sector. (4 Bytes) */

    off_t length;      /* File size in bytes. (4 Bytes) */
    unsigned magic;    /* Magic number.(4 Bytes) */
    uint8_t unused[3]; /* Not used. (Each Number is 1 byte)*/

    /* Subdirectories*/
    bool isDir; /* Flag to note if regular file (0) or directory (1) | (1 Byte) */

    /* Extensible Files, from the the VIDEO */
    block_sector_t direct_map_table[NUMBER_DIRECT_BLOCKS]; /* Direct Mappings of blocks, max 123 blocks/sectors (492 Bytes)*/
    block_sector_t indirect_block_sec;                     /* Indirect mapping (4 Bytes) */
                                                           // block_sector_t double_indirect_block_sec;              /* Double indirect mapping (4 Bytes) */
};

/* In-memory inode. */
struct inode
{
    struct list_elem elem;        /* Element in inode list. */
    block_sector_t sector;        /* Sector number of disk location. */
    int open_cnt;                 /* Number of openers. */
    bool removed;                 /* True if deleted, false otherwise. */
    int deny_write_cnt;           /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;       /* Inode content. */
    struct lock lock;             /* A lock to sync per file, instead of per */
    block_sector_t parent_sector; /* the parent of this sector */
};

/* Indirect Block, points to other sectors */
struct indirect_block
{
    block_sector_t blocks[NUMBER_INDIRECT_BLOCKS_PER_SECTOR];
};

void inode_init(void);
bool inode_create(block_sector_t, off_t, bool is_dir);
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);

/* NEW FUNCTIONS*/
block_sector_t get_inode_parent(const struct inode *inode);
bool set_inode_parent(block_sector_t parent, block_sector_t child);

#endif /* filesys/inode.h */
