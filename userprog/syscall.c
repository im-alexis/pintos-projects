#include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
    /* Remove these when implementing syscalls */
    printf("system call!\n");
    // 2. given an interupt frame, arguments go to stack. so grab it and the arg goes on 0
    // do a giant switch case statement
    uint32_t *esp = f->esp;

    switch (*esp)
    {
    case SYS_HALT:

        break;
    case SYS_EXIT:

        break;
    case SYS_EXEC:

        break;
    case SYS_WAIT:

        break;
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

    thread_exit();
}
