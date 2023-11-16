#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct exec_files
{
    struct file_plus file;
    struct list_elem elem;      /* List element for all exec files list. */
    struct list exec_file_list; /* So that the thread can access all other threads*/
};
tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

#endif /* userprog/process.h */
