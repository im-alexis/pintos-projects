#include <debug.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lib/stdio.h"
#include "filesys/filesys.h"
#include "vm/page.h"

#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

#include <log.h>

/*

* MASSIVE CODE REHAUL WITH PLUS FILES

*/
int search_by_filename_plus(char *target_file)
{
    struct thread *cur = thread_current();
    for (int i = 2; i < MAX_FD; i++)
    {
        if (!strcmp(cur->file_descriptor_table_plus[i]->name, target_file))
        {
            return i;
        }
    }
    return -1;
}

bool removed_from_table_by_filename_plus(char *filename)
{
    struct thread *cur = thread_current();
    for (int i = 2; i < MAX_FD; i++)
    {
        struct file_plus *looped_file = cur->file_descriptor_table_plus[i];
        if (!strcmp(looped_file->name, filename))
        {
            /*
            Properly rmeove from table
            */
            // free(cur->file_descriptor_table[i]);
            cur->file_descriptor_table_plus[i] = NULL;
            cur->how_many_fd_plus--;
            return true;
        }
    }

    return false;
}

struct file_plus *create_file_plus(struct file *file, char *filename)
{
    struct file_plus *new_file = (struct file_plus *)malloc(sizeof(struct file_plus));
    new_file->file = file;
    memcpy(new_file->name, filename, sizeof(filename));
}

void removed_from_table_plus(int fd)
{
    struct thread *cur = thread_current();
    cur->file_descriptor_table_plus[fd] = NULL;
    cur->how_many_fd_plus--;
}

int find_next_table_pos_plus();
int find_next_table_pos_plus()
{
    struct thread *cur = thread_current();
    for (int i = 2; i < MAX_FD; i++)
    {
        if (cur->file_descriptor_table_plus[i] == NULL)
        {
            cur->fdt_index_plus = i;
            return i;
        }
    }
    return -1;
}

int add_to_table_plus(struct file_plus *new_file)
{
    struct thread *cur = thread_current();
    // add like a loop around
    if ((cur->fdt_index_plus == MAX_FD - 1) && (cur->how_many_fd_plus != MAX_FD))
    {
        find_next_table_pos_plus();
    }
    else if (new_file != NULL && (cur->how_many_fd_plus < MAX_FD))
    {
        int val = cur->fdt_index_plus;

        cur->file_descriptor_table_plus[val] = new_file;
        cur->fdt_index_plus++;
        cur->how_many_fd_plus++;
        return val;
    }
    return -1;
}

void destroy_plus_file(struct file_plus *pfile, bool close_file)
{
    if (close_file)
    {
        file_close(pfile->file);
    }
    free(pfile);
}