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
    int len = strlen(pathname);
    char path[len + 1];
    memcpy(path, pathname, len + 1);
    // char *file_name;

    struct dir *dir;
    if (path[0] == '/' || !thread_current()->current_dir)
    {
        /* If the string starts w/ '/' then is an abs path */
        dir = dir_open_root();
        log(L_INFO, "using root directory [%08x]", dir->inode);
    }
    else
    {
        /* else its a relative path */
        dir = dir_reopen(thread_current()->current_dir);
        log(L_INFO, "using current directory [%08x]", dir->inode);
    }

    char *cur_str, *sav_ptr, *prev_str;
    prev_str = strtok_r(path, "/", &sav_ptr);
    for (cur_str = strtok_r(NULL, "/", &sav_ptr); cur_str != NULL; prev_str = cur_str, cur_str = strtok_r(NULL, "/", &sav_ptr))
    {
        log(L_DEBUG, "prev_str: [%s]", prev_str);
        struct inode *inode;
        if (!strcmp(prev_str, "."))
            continue; /* Its a path staring from the current directory, keep looking*/
        else if (!strcmp(prev_str, ".."))
        {
            /* Check if the directory is NULL */
            if (dir == NULL)
                return NULL;
            /* open the inode of the parent sector*/
            block_sector_t parent_sector = dir->inode->parent_sector;
            inode = inode_open(parent_sector);
            if (inode == NULL)
            {
                return NULL;
            }
        }
        else if (!dir_lookup(dir, prev_str, &inode)) /* Look to see if the directory exist*/
            return NULL;

        if (inode->data.isDir) /* Check to see if the inode is a directory*/
        {
            dir_close(dir);
            dir = dir_open(inode);
        }
        else
        {
            // file_name = prev_str;
            //  strcpy(filename, prev_str);
            inode_close(inode);
        }
    }
    // file_name = prev_str;
    return dir;
}

char *get_filename(char *pathname)
{
    int len = strlen(pathname);
    char path[len + 1];
    memcpy(path, pathname, len + 1);

    char *cur_str, *sav_ptr, *prev_str = "";
    for (cur_str = strtok_r(path, "/", &sav_ptr); cur_str != NULL; cur_str = strtok_r(NULL, "/", &sav_ptr))
        prev_str = cur_str;

    char *name = malloc(strlen(prev_str) + 1);
    memcpy(name, prev_str, strlen(prev_str) + 1);
    return name;
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
    log(L_TRACE, "filesys_create(name: [%s], initial_size: [%d], is_dir [%d] )", name, initial_size, is_dir);
    block_sector_t inode_sector = 0;
    // struct dir *dir = dir_open_root();
    char *filename = get_filename(name);
    struct dir *dir = parse_path_dir(name);

    /* If the directory is in use maybe*/
    if (dir->inode->removed)
    {
        log(L_ERROR, "Directory was remove, cannot create the file");
        return false;
    }

    log(L_DEBUG, "name: [%s] | filename: [%s] | dir: [%08x] ", name, filename, dir->inode);
    bool success = (dir != NULL && free_map_allocate(1, &inode_sector) && inode_create(inode_sector, initial_size, is_dir) && dir_add(dir, filename, inode_sector));

    if (!success && inode_sector != 0)
    {
        free_map_release(inode_sector, 1);
    }
    // if (is_dir)
    // {
    /* Trying to add the "." and ".." to the directory, not sure if totally needed*/
    //     dir_add();
    //     dir_add();
    // }
    free(filename);
    dir_close(dir);
    log(L_DEBUG, "file_creation_success_flag: [%d]", success);
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
    log(L_TRACE, "filesys_open(name: [%s])", name);
    char *filename = get_filename(name);
    struct dir *dir = parse_path_dir(name);
    struct inode *inode = NULL;
    bool val = false;
    // log(L_DEBUG, "filename: [%s] | dir: [%08x]", filename, dir->inode);
    if (dir != NULL && strcmp(filename, ""))
    {
        val = dir_lookup(dir, filename, &inode);
        dir_close(dir);
        free(filename);
    }
    else if (dir != NULL && !strcmp(filename, ""))
    {
        log(L_INFO, "Opening the directory itself");
        free(filename);
        return file_open(dir->inode);
    }

    log(L_INFO, "Opening a file/Directory in a directory | dir_lookup_result: [%d]", val);
    return file_open(inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool filesys_remove(const char *name)
{
    char *filename = get_filename(name);
    struct dir *dir = parse_path_dir(name);
    bool success = dir != NULL && dir_remove(dir, filename);

    dir_close(dir);
    free(filename);
    return success;
}

/*
* Given a file_name/path change the directory
 WORK IN PROGRESS
*/
bool filesys_chdir(const char *name)
{
    log(L_TRACE, "filesys_chdir(name: [%s])", name);
    char *filename = get_filename(name);
    struct dir *dir = parse_path_dir(name);
    struct thread *cur = thread_current();

    struct inode *inode = NULL;

    if (dir == NULL)
    {
        free(filename);
        return false;
    }
    /* Special Case: use the parent directory */
    else if (!strcmp(filename, ".."))
    {
        /* Check if the directory is NULL */
        if (dir == NULL)
            return NULL;
        /* open the inode of the parent sector*/
        block_sector_t parent_sector = dir->inode->parent_sector;
        inode = inode_open(parent_sector);
        if (inode == NULL)
        {
            free(name);
            return false;
        }
    }
    /* Special Case: use the current directory */
    else if (!strcmp(filename, ".") || (strlen(filename) == 0 && is_root_dir(dir)))
    {
        cur->current_dir = dir;
        free(filename);
        return true;
    }
    else
        dir_lookup(dir, filename, &inode);

    dir_close(dir);

    // now open up target dir
    dir = dir_open(inode);

    if (dir == NULL)
    {
        free(filename);
        return false;
    }
    else
    {
        dir_close(cur->current_dir);
        cur->current_dir = dir;
        free(filename);
        return true;
    }
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
