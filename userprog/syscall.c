#include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
    /* Remove these when implementing syscalls */
    // printf("system call!\n");
    // 2. given an interupt frame, arguments go to stack. so grab it and the arg goes on 0

    // return values should be place on register eax
    uint32_t *esp = f->esp;
    uint32_t syscall_num = *esp;

    /*

    NEED TO VALIDATE POINTERS

    */

    switch (syscall_num)
    {

    /*
    PROCESS RELATED SYSCALLS
    */
    case SYS_HALT:
        f->eax = shutdown_power_off();
        break;
    case SYS_EXIT:

        // Not really sure how to do this

        int status = *(esp + 4);
        // process_exit();
        thread_exit();
        break;
    case SYS_EXEC:
    {
        char *file_name = esp + 4;
        f->eax = process_execute(file_name);

        break;
    }

    case SYS_WAIT:
    {
        /*
            NEED WAIT RULES VALIDATION
            ie.
            1. only call wait once (could add a param (has_been_waited_on) in thread struct)
            2. only wait on your own children (check if the struct has that list, if not add it)

        */
        tid_t tid = esp + 4;
        f->eax = process_wait(tid);
        break;
    }

    /*
    FILE RELATED SYSCALLS
    LOOK AT /filesys/filesys.c for the calls

    struct file

    NEED TO CREATE A FILE DESCRIPTOR TABLE (possibly in the thread struct)
    */
    case SYS_CREATE:

        break;
    case SYS_REMOVE:

        break;

    case SYS_OPEN:

        break;
    case SYS_FILESIZE:

        break;
    case SYS_READ:

        break;
    case SYS_WRITE:

        break;
    case SYS_SEEK:

        break;
    case SYS_TELL:

        break;
    case SYS_CLOSE:

        break;
    default:
        break;

        /*
        DID NOT INCLUDES THESE Yet
        Project 3 and optionally project 4.
            SYS_MMAP,        Map a file into memory.
            SYS_MUNMAP,  Remove a memory mapping.

        Project 4 only.
            SYS_CHDIR,   Change the current directory.
            SYS_MKDIR,   Create a directory.
            SYS_READDIR,  Reads a directory entry.
            SYS_ISDIR,   Tests if a fd represents a directory.
            SYS_INUMBER  Returns the inode number for a fd.

         */
    }

    // thread_exit();
}
