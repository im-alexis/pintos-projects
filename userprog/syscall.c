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

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user(const uint8_t *uaddr)
{
    int result;
    asm("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a"(result) : "m"(*uaddr));
    return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user(uint8_t *udst, uint8_t byte)
{
    int error_code;
    asm("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a"(error_code), "=m"(*udst) : "q"(byte));
    return error_code != -1;
}

/*
This function will be used to validate pointers using the get_user/put_user method

args:
    *addy (the address you are trying to read/write),
    byte (for put_user, can be anything if trying to read)
    size (if you can write/read at 0 and n-1 then it can be done all throughout)
    type_of_call (0 for a read and 1 for write)


*/
bool valid_ptr(uint8_t *addy, uint8_t byte, int size, uint8_t type_of_call);

bool valid_ptr(uint8_t *addy, uint8_t byte, int size, uint8_t type_of_call)
{
    if (type_of_call == 1)
    {
        bool ret1 = put_user(addy, byte);
        bool ret2 = put_user(addy, byte);
        return (ret1 && ret2);
    }
    else if (type_of_call == 0)
    {
        int ret1 = get_user(addy);
        int ret2 = get_user(addy + (size - 1));
        if (ret1 != -1 && ret2 != -1)
            return true;
        else
            return false;
    }
    else
        return false;
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
    if (!valid_ptr(esp, -1, 1, 1))
    {
        return;
    }

    switch (syscall_num)
    {

    /*
    PROCESS RELATED SYSCALLS
    */
    case SYS_HALT:
    {
        f->eax = shutdown_power_off();
        break;
    }

    case SYS_EXIT:
    {
        struct thread *cur = thread_current();
        cur->exit_code = *(esp + 4);
        // process_exit();
        thread_exit();
        break;
    }

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
    {
        break;
    }

    case SYS_REMOVE:
    {
        break;
    }

    case SYS_OPEN: // call file_open()
    {
        break;
    }

    case SYS_FILESIZE: // file_length();
    {
        break;
    }

    case SYS_READ: // validate with write check index [0] and [size-1] -> put_user()

    {
        break;
    }

    case SYS_WRITE: // validate with read check index [0] and [size-1] -> get_user()
    {
        break;
    }
    case SYS_SEEK: // file_seek()
    {
        break;
    }

    case SYS_TELL: // file_seek();
    {
        break;
    }

    case SYS_CLOSE: // file_close()
    {
        break;
    }

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
