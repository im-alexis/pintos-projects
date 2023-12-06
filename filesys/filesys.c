#include <debug.h>
#include <stdio.h>
#include <string.h>

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"

#include "threads/thread.h"
#define LOGGING_LEVEL 6
#include <log.h>

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/*
Given a pathname, opens the directory to that name.
*/
struct dir *parse_path_dir(char *pathname)
{
    log(L_TRACE, "parse_path_dir(pathname: [%s])", pathname);
    int length = strlen(pathname);
    char path[length + 1];
    memcpy(path, pathname, length + 1);

    struct dir *dir;
    if (path[0] == '/' || !thread_current()->current_dir)
    {
        /* If the string starts w/ '/' then is an abs path */
        dir = dir_open_root();
        log(L_DEBUG, "using root directory [%08x]", dir->inode);
    }
    else
    {
        /* else its a relative path */
        dir = dir_reopen(thread_current()->current_dir);
        log(L_DEBUG, "using current directory [%08x]", dir->inode);
    }

    char *cur_str, *sav_ptr, *prev_str;
    prev_str = strtok_r(path, "/", &sav_ptr);
    for (cur_str = strtok_r(NULL, "/", &sav_ptr); cur_str != NULL; prev_str = cur_str, cur_str = strtok_r(NULL, "/", &sav_ptr))
    {
        log(L_DEBUG, "prev_str: [%s])", prev_str);
        struct inode *inode;
        if (!strcmp(prev_str, "."))
            continue; /* Its a path staring from the current directory, keep looking*/
        else if (!strcmp(prev_str, ".."))
        {
            /*
             Its looking for the parent of the current directory
             & NEED A FUNCTION TO GET A PARENT DIRECTORY INODE
            */
        }
        else if (!dir_lookup(dir, prev_str, &inode)) /* Look to see if the directory exist*/
            return NULL;

        if (inode->data.isDir) /* Check to see if the inode is a directory*/
        {
            dir_close(dir);
            dir = dir_open(inode);
        }
        else
            inode_close(inode);
    }

    return dir;
}

/*
Given a file and directory, opens the file to that name.
*/
struct file *dir_file_open(char *file_name, struct dir *directory)
{
}

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void filesys_init(bool format)
{
    fs_device = block_get_role(BLOCK_FILESYS);
    if (fs_device == NULL)
    {
        PANIC("No file system device found, can't initialize file system.");
    }

    inode_init();
    free_map_init();

    if (format)
    {
        do_format();
    }

    free_map_open();
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void filesys_done(void)
{
    free_map_close();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool filesys_create(const char *name, off_t initial_size, bool is_dir)
{
    log(L_TRACE, "filesys_create(name: [%s], initial_size: [%d] )", name, initial_size);
    block_sector_t inode_sector = 0;
    // struct dir *dir = dir_open_root();
    struct dir *dir = parse_path_dir(name);
    bool success = (dir != NULL && free_map_allocate(1, &inode_sector) && inode_create(inode_sector, initial_size, is_dir) && dir_add(dir, name, inode_sector));
    log(L_DEBUG, "success: [%d]", success);

    if (!success && inode_sector != 0)
    {
        free_map_release(inode_sector, 1);
    }
    dir_close(dir);

    return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open(const char *name)
{
    struct dir *dir = dir_open_root();
    struct inode *inode = NULL;

    if (dir != NULL)
    {
        dir_lookup(dir, name, &inode);
    }
    dir_close(dir);

    return file_open(inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool filesys_remove(const char *name)
{
    struct dir *dir = dir_open_root();
    bool success = dir != NULL && dir_remove(dir, name);

    dir_close(dir);

    return success;
}

/* Formats the file system. */
static void
do_format(void)
{
    printf("Formatting file system...");
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16))
    {
        PANIC("root directory creation failed");
    }
    free_map_close();
    printf("done.\n");
}
