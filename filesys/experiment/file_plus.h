#ifndef FILESYS_EXPERIMENT_FILE_PLUS_H
#define FILESYS_EXPERIMENT_FILE_PLUS_H
#include "filesys/file.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lib/kernel/hash.h"
#include "vm/frame.h"
#include "filesys/directory.h"
#include "filesys/file.h"
/*
? Not sure, trying to fix old stuff, could be helpful
*/

struct file_plus
{
    struct file *file;
    char *name;
    bool writable;
};

struct file_plus *create_file_plus(struct file *file, char *filename);
bool removed_from_table_by_filename_plus(char *filename);
int search_by_filename_plus(char *target_file);
void removed_from_table_plus(int fd);
int add_to_table_plus(struct file_plus *new_file);
void destroy_plus_file(struct file_plus *pfile, bool close_file);
#endif /* filesys/file.h */