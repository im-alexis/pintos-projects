#include <debug.h>
#include <round.h>
#include <string.h>

#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#define LOGGING_LEVEL 6
#include <log.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

static bool inode_alloc(struct inode_disk *inode_disk);
static bool inode_save(struct inode_disk *inode_disk, off_t length);
static bool inode_dealloc(struct inode *inode);

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/*
Given an disk inode and and index, it returns the sector
returns -1 if went wrong
*/
static block_sector_t idx_sect(const struct inode_disk *inode_disk, off_t index)
{
    log(L_TRACE, "idx_sect(inode_disk: [%08x], index: [%d])", inode_disk, index);
    off_t index_start = 0;
    block_sector_t sector;

    /* Direct Mappings*/
    off_t index_max = NUMBER_DIRECT_BLOCKS;
    if (index < index_max)
    {
        sector = inode_disk->direct_map_table[index];
        log(L_DEBUG, "Direct Mapping | Block Sector: [%d] ", sector);
        return sector;
    }
    index_start = index_max;

    /* Using the single Indirect Block, so add the new limit to index_max + 128 */
    index_max += NUMBER_INDIRECT_BLOCKS_PER_SECTOR;
    if (index < index_max)
    {
        struct indirect_block *indirect_block;
        indirect_block = calloc(1, sizeof(struct indirect_block));
        block_read(fs_device, inode_disk->indirect_block_sec, indirect_block); /* Read what is ther everything to disk */
        log(L_DEBUG, "Single Indirect | Block Sector: [%d] ", sector);
        sector = indirect_block->blocks[index - index_start];
        free(indirect_block);
        return sector;
    }
    index_start = index_max;

    /* Using the Double Indirect Block, so add the new limit to index_max (128*128) */
    index_max += NUMBER_BLOCKS_D_INDIRECT;
    if (index < index_max)
    {
        /* Getting the index of both levels */
        off_t index_first = (index - index_start) / NUMBER_INDIRECT_BLOCKS_PER_SECTOR;  /* Gets the block it is on in first lvl*/
        off_t index_second = (index - index_start) % NUMBER_INDIRECT_BLOCKS_PER_SECTOR; /* Gets the index on the second lvl*/
        /* Read what is there*/
        struct indirect_block *indirect_block;
        indirect_block = calloc(1, sizeof(struct indirect_block));
        block_read(fs_device, inode_disk->double_indirect_block_sec, indirect_block); /* Read what is ther everything to disk */
        block_read(fs_device, indirect_block->blocks[index_first], indirect_block);
        log(L_DEBUG, "Double Indirect | Index 1: [%d] | Index 2: [%d] | Block Sector: [%d] ", index_first, index_second, sector);
        sector = indirect_block->blocks[index_second];
        free(indirect_block);
        return sector;
    }
    /* if everything went wrong ie a file too big */
    log(L_ERROR, "Everything when wrong");
    return -1;
}

/* Returns the block device sector that contains byte offset POS
 * within INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */

static block_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
    /*
     * Modify, for file Extension
     * Chnage file offset to block address, is what the video said
     */
    ASSERT(inode != NULL);
    if (0 <= pos && pos < inode->data.length)
    {
        /*Sector Index*/
        off_t index = pos / BLOCK_SECTOR_SIZE;
        return idx_sect(&inode->data, index);
    }
    else
        return -1;
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void)
{
    list_init(&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * device.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */

bool inode_create(block_sector_t sector, off_t length, bool is_dir)
{
    log(L_TRACE, "inode_create(sector: [%d], length: [%d], is_dir [%d])", sector, length, is_dir);
    /*
     * Modify, for file Extension
     */
    struct inode_disk *disk_inode = NULL;
    bool success = false;

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL)
    {
        disk_inode->length = length;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->isDir = is_dir;
        if (inode_alloc(disk_inode))
        {
            /* Write from the disk */
            block_write(fs_device, sector, disk_inode);
            success = true;
        }
        free(disk_inode);
    }
    log(L_DEBUG, "inode_creation_success_flag: [%d]", success);
    return success;
}

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e))
    {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector)
        {
            inode_reopen(inode);
            return inode;
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
    {
        return NULL;
    }

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    lock_init(&inode->lock);
    /* Read from the disk */
    block_read(fs_device, inode->sector, &inode->data);
    return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
    if (inode != NULL)
    {
        inode->open_cnt++;
    }
    return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
    return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode)
{

    /* Ignore null pointer. */
    if (inode == NULL)
    {
        return;
    }

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0)
    {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);

        /* Deallocate blocks if removed. */
        if (inode->removed)
        {
            free_map_release(inode->sector, 1);
            inode_dealloc(inode);
        }

        free(inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void inode_remove(struct inode *inode)
{
    ASSERT(inode != NULL);
    inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
    log(L_TRACE, "inode_read_at(inode: [%08x], size: [%d], offset: [%d] )", inode, size, offset);
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    uint8_t *bounce = NULL;

    while (size > 0)
    {
        log(L_DEBUG, "Size:[%d]", size);
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
        {
            break;
        }

        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
            /* Read full sector directly into caller's buffer. */
            block_read(fs_device, sector_idx, buffer + bytes_read);
        }
        else
        {
            /* Read sector into bounce buffer, then partially copy
             * into caller's buffer. */
            if (bounce == NULL)
            {
                bounce = malloc(BLOCK_SECTOR_SIZE);
                if (bounce == NULL)
                {
                    break;
                }
            }
            block_read(fs_device, sector_idx, bounce);
            memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }
    free(bounce);

    return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
                     off_t offset)
{
    // log(L_TRACE, "inode_write_at(inode: [%08x], size: [%d], offset: [%d] )", inode, size, offset);
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    uint8_t *bounce = NULL;

    if (inode->deny_write_cnt)
        return 0;
    lock_acquire(&inode->lock);
    /* For Writing beyond EOF*/
    if (byte_to_sector(inode, offset + size - 1) == -1u)
    {
        bool success;
        success = inode_save(&inode->data, offset + size);
        if (!success)
        {
            lock_release(&inode->lock);
            return 0;
        }
        /* Write Back to Disk */
        inode->data.length = offset + size;
        block_write(fs_device, inode->sector, &inode->data);
    }

    while (size > 0)
    {
        /* Sector to write, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
        {
            break;
        }

        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
            /* Write full sector directly to disk. */
            block_write(fs_device, sector_idx, buffer + bytes_written);
        }
        else
        {
            /* We need a bounce buffer. */
            if (bounce == NULL)
            {
                bounce = malloc(BLOCK_SECTOR_SIZE);
                if (bounce == NULL)
                {
                    break;
                }
            }

            /* If the sector contains data before or after the chunk
             * we're writing, then we need to read in the sector
             * first.  Otherwise we start with a sector of all zeros. */
            if (sector_ofs > 0 || chunk_size < sector_left)
            {
                block_read(fs_device, sector_idx, bounce);
            }
            else
            {
                memset(bounce, 0, BLOCK_SECTOR_SIZE);
            }
            memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
            block_write(fs_device, sector_idx, bounce);
        }

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }

    free(bounce);
    // log(L_DEBUG, "bytes written: [%d]", bytes_written);
    lock_release(&inode->lock);
    return bytes_written;
}

/* Disables writes to INODE.
 * May be called at most once per inode opener. */
void inode_deny_write(struct inode *inode)
{
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode *inode)
{
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode)
{
    return inode->data.length;
}

/*
Given an inode, it return the parent sector of an inode. ie like the parent directory
*/
block_sector_t get_inode_parent(const struct inode *inode)
{
    return inode->parent_sector;
}

/*
Sets the parent blocks for an inode
*/
bool set_inode_parent(block_sector_t parent, block_sector_t child)
{
    struct inode *inode = inode_open(child);
    if (!inode)
        return false;

    inode->parent_sector = parent;
    inode_close(inode);
    return true;
}

/* & ADDED */

/*
Given a pointer to an inode_disk, it allocates some blocks for it
*/
static bool inode_alloc(struct inode_disk *inode_disk)
{

    log(L_TRACE, "inode_alloc(inode_disk: [%08x])", inode_disk);
    return inode_save(inode_disk, inode_disk->length);
}

/*
Given a pointer to an inode_disk, it allocates some blocks for it
*/
bool inode_save_indirect(block_sector_t *blk_ptr, size_t num_sectors, int lvl)
{
    log(L_TRACE, "inode_save_indirect(blk_ptr: [%08x], num_sectors: [%d], level: [%d] )", blk_ptr, num_sectors, lvl);
    static char zero[BLOCK_SECTOR_SIZE]; /* NEED in bytes */

    ASSERT(lvl <= 2);

    if (lvl == 0)
    {
        if (*blk_ptr == 0)
        {
            if (!free_map_allocate(1, blk_ptr))
            {
                return false;
            }
            /* Write everything to the disk */
            block_write(fs_device, *blk_ptr, zero);
        }
        return true;
    }
    struct indirect_block indirect_block;
    if (*blk_ptr == 0)
    {
        free_map_allocate(1, blk_ptr);
        /* Write everything to the disk */
        block_write(fs_device, *blk_ptr, zero);
    }
    /* Read from the disk */
    block_read(fs_device, *blk_ptr, &indirect_block);

    size_t num = (lvl == 1 ? 1 : NUMBER_INDIRECT_BLOCKS_PER_SECTOR);
    size_t i, j = DIV_ROUND_UP(num_sectors, num);

    for (i = 0; i < j; ++i)
    {
        size_t sub = num_sectors < num ? num_sectors : num;
        if (!inode_save_indirect(&indirect_block.blocks[i], sub, lvl - 1))
        {
            return false;
        }
        num_sectors -= sub;
    }
    ASSERT(num_sectors == 0);
    /* Write back to the disk */
    block_write(fs_device, *blk_ptr, &indirect_block);
    return true;
}

static bool inode_save(struct inode_disk *inode_disk, off_t len)
{
    log(L_TRACE, "inode_save(disk_inode: [%08x], length: [%d )", inode_disk, len);
    static char zero[BLOCK_SECTOR_SIZE];
    if (len < 0)
    {
        log(L_ERROR, "Negative Length");
        return false;
    }

    size_t num_sectors = bytes_to_sectors(len);
    size_t i;
    log(L_INFO, "num_sectors[%d]", num_sectors);
    /* Direct Mappings */
    size_t j = num_sectors < NUMBER_DIRECT_BLOCKS ? num_sectors : NUMBER_DIRECT_BLOCKS;
    for (i = 0; i < j; ++i)
    {
        if (inode_disk->direct_map_table[i] == 0)
        {
            if (!free_map_allocate(1, &inode_disk->direct_map_table[i]))
            {
                log(L_ERROR, "could not allocate");
                return false;
            }
            /* Write to the disk */
            block_write(fs_device, inode_disk->direct_map_table[i], zero);
        }
    }
    num_sectors -= j;
    if (num_sectors == 0)
    {
        log(L_INFO, "Done, Direct Mapping");
        return true;
    }

    /* Single Indirect Block*/

    j = num_sectors < NUMBER_INDIRECT_BLOCKS_PER_SECTOR ? num_sectors : NUMBER_INDIRECT_BLOCKS_PER_SECTOR;
    if (!inode_save_indirect(&inode_disk->indirect_block_sec, j, 1))
    {
        log(L_ERROR, "could not allocate");
        return false;
    }

    num_sectors -= j;
    if (num_sectors == 0)
    {
        log(L_INFO, "Done, Single Indirect Block ");
        return true;
    }

    /* Double Indirect Block */
    j = num_sectors < NUMBER_BLOCKS_D_INDIRECT ? num_sectors : NUMBER_BLOCKS_D_INDIRECT;

    if (!inode_save_indirect(&inode_disk->double_indirect_block_sec, j, 2))
    {
        log(L_ERROR, "could not allocate");
        return false;
    }
    num_sectors -= j;
    if (num_sectors == 0)
    {
        log(L_INFO, "Done, Double Indirect Block ");
        return true;
    }

    ASSERT(num_sectors == 0);
    log(L_ERROR, "IT went fully poopy");
    return false;
}

static void inode_dealloc_indirect(block_sector_t blk_sector, size_t num_sectors, int lvl)
{
    log(L_TRACE, "inode_dealloc_indirect(blk_sector: [%d], num_sectors: [%d], level: [%d] )", blk_sector, num_sectors, lvl);
    /* Check that it less than two lvls, we only do up to two lvls*/
    ASSERT(lvl <= 2);
    if (lvl == 0)
    {
        free_map_release(blk_sector, 1);
        return;
    }

    struct indirect_block indirect_block;
    /* Read from the disk */
    block_read(fs_device, blk_sector, &indirect_block);

    size_t num = (lvl == 1 ? 1 : NUMBER_INDIRECT_BLOCKS_PER_SECTOR);
    size_t i, j = DIV_ROUND_UP(num_sectors, num);

    for (i = 0; i < j; ++i)
    {
        size_t sub = num_sectors < num ? num_sectors : num;
        inode_dealloc_indirect(indirect_block.blocks[i], sub, lvl - 1);
        num_sectors -= sub;
    }

    ASSERT(num_sectors == 0);
    free_map_release(blk_sector, 1);
}

static bool inode_dealloc(struct inode *inode)
{
    log(L_TRACE, "inode_dealloc(inode: [%08x])", inode);
    off_t file_length = inode->data.length;
    if (file_length < 0)
    {
        return false;
    }
    size_t num_sectors = bytes_to_sectors(file_length);
    size_t i, j;

    /* Direct Mapping */
    j = num_sectors < NUMBER_DIRECT_BLOCKS ? num_sectors : NUMBER_DIRECT_BLOCKS;
    for (i = 0; i < j; ++i)
    {
        free_map_release(inode->data.direct_map_table[i], 1);
    }
    num_sectors -= j;

    /* Single Indirect Block */
    j = num_sectors < NUMBER_INDIRECT_BLOCKS_PER_SECTOR ? num_sectors : NUMBER_INDIRECT_BLOCKS_PER_SECTOR;
    if (j > 0)
    {
        inode_dealloc_indirect(inode->data.indirect_block_sec, j, 1);
        num_sectors -= j;
    }

    /* Double Indirect Block */
    j = num_sectors < (NUMBER_BLOCKS_D_INDIRECT) ? num_sectors : (NUMBER_BLOCKS_D_INDIRECT);
    if (j > 0)
    {
        inode_dealloc_indirect(inode->data.double_indirect_block_sec, j, 2);
        num_sectors -= j;
    }

    ASSERT(num_sectors == 0);
    return true;
}