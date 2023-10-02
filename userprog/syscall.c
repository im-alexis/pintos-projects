// #include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "lib/stdio.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"

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
Terminates a thread with exit code -1
*/
void matelo();
void matelo(struct thread *cur)
{
    cur->exit_code = -1;
    thread_exit();
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
    static struct lock file_lock; // lock for synch when doing file related stuff
    lock_init(&file_lock);

    struct thread *cur = thread_current();

    // return values should be place on register eax
    uint32_t *esp = f->esp; //
    uint32_t syscall_num = *esp;
    //  uint32_t *addy = f->esp;

    /*
    Don't know if they work - needs testing
    */
    uint32_t *arg0 = esp + 1;
    uint32_t *arg1 = esp + 2;
    uint32_t *arg2 = esp + 3;

    /*

    NEED TO TEST VALIDATE POINTERS
    Look thru
    - is_user_vaddr()
    - get_user()
    - put_user()

    */

    /*
    this section is for some error handling
    if you encounter any error give it an auto boot (-1)
    */

    if (!is_user_vaddr(esp))
    {
        matelo(cur);
        return;
    }
    if (!(is_user_vaddr(esp + 1) && is_user_vaddr(esp + 2) && is_user_vaddr(esp + 3)))
    {
        matelo(cur);
        return;
    }
    if (*esp < SYS_HALT || *esp > SYS_INUMBER)
    {
        matelo(cur);
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
        int exit_code = ((int)*arg0);
        cur->exit_code = ((int)*arg0);
        thread_exit();
        break;
    }

    case SYS_EXEC:
    {
        char *file_name = ((const char *)*arg0);
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

    Buzzwords : file_deny_write
    might want to have a lock for when opening, writing etc.

    */
    case SYS_CREATE:
    {
        char *file = ((const char *)*arg0);
        unsigned size = ((unsigned)*arg1);
        struct file *new_file = malloc(sizeof(struct file));
        lock_acquire(&file_lock);
        f->eax = filesys_create(file, size);
        lock_release(&file_lock);
        break;
    }

    case SYS_REMOVE:
    {
        // search for a file in descriptor table
        char *file = ((const char *)*arg0);
        lock_acquire(&file_lock);
        bool ret = removed_from_table(file, cur);
        lock_release(&file_lock);
        if (ret)
        {
            lock_acquire(&file_lock);
            filesys_remove(file);
            lock_release(&file_lock);

            f->eax = true;
        }
        else
        {
            f->eax = false;
        }
        break;
    }
        // idk what we are doing, we are drunk, sober alexis, look here plz
        // javier approved of this one,,

    case SYS_FILESIZE: // file_length();
    {
        int fd = ((int)*arg0);
        if ((fd != STDIN_FILENO) && (fd != STDOUT_FILENO))
        {
            struct file *target = cur->file_descriptor_table[fd];
            if (target != NULL)
            {
                lock_acquire(&file_lock);
                f->eax = file_length(target);
                lock_release(&file_lock);
            }
        }
        else
        {
            f->eax = 0;
        }

        break;
    }

    case SYS_READ: // validate with write check index [0] and [size-1] -> put_user()

    {
        int fd = ((int)*arg0);
        char *buffer = ((char *)*arg1);
        uint32_t size = (uint32_t)*arg2;

        break;
    }

    case SYS_WRITE: // validate with read check index [0] and [size-1] -> get_user()
    {
        /*
        Need some validaiton -

       rough synch attempt

        */
        int fd = ((int)*arg0);
        char *buffer = ((char *)*arg1);
        unsigned size = (unsigned)*arg2;
        bool result = valid_ptr(&buffer, -1, (int)size, 0);
        if (!result)
        {
            f->eax = 0;
            matelo(cur);
            return;
        }

        if (fd == STDIN_FILENO)
        {
            f->eax = 0;
            matelo(cur);
            return;
        }
        else if (fd == STDOUT_FILENO)
        {

            lock_acquire(&file_lock);
            putbuf(buffer, size); // writes to the console
            lock_release(&file_lock);
            f->eax = size;
        }
        else
        {
            struct file *targeta = cur->file_descriptor_table[fd];
            if (targeta == NULL)
            {

                matelo(cur);
                return;
            }
            lock_acquire(&file_lock);
            file_write(targeta, buffer, size);
            lock_release(&file_lock);
            f->eax = size;
        }

        break;
    }
    case SYS_SEEK: // file_seek()
    {
        int fd = ((int)*arg0);
        unsigned size = (unsigned)*arg1;
        if (fd == STDIN_FILENO)
        {
            // need to get the file for stdin
            lock_acquire(&file_lock);
            // call file_seek()
            lock_release(&file_lock);
        }
        else if (fd == STDOUT_FILENO)
        {
            // need to get the file for stdout
            lock_acquire(&file_lock);
            // call file_seek()
            lock_release(&file_lock);
        }
        else
        {
            struct file *target = cur->file_descriptor_table[fd];
            if (target != NULL)
            {
                lock_acquire(&file_lock);
                file_seek(target, size);
                lock_release(&file_lock);
            }
        }

        break;
    }

    case SYS_TELL: // file_tell();
    {
        int fd = ((int)*arg0);
        if (fd == STDIN_FILENO)
        {
            // need to get the file for stdin
            lock_acquire(&file_lock);
            // call file_tell()
            lock_release(&file_lock);
        }
        else if (fd == STDOUT_FILENO)
        {
            // need to get the file for stdout
            lock_acquire(&file_lock);
            // call file_tell()
            lock_release(&file_lock);
        }
        else
        {
            struct file *target = cur->file_descriptor_table[fd];
            if (target != NULL)
            {
                lock_acquire(&file_lock);
                f->eax = file_tell(target);
                lock_release(&file_lock);
            }
            else
            {
                matelo(cur);
                return;
            }
        }
        break;
    }

    case SYS_CLOSE: // file_close()
    {
        int fd = ((int)*arg0);
        if (fd == STDIN_FILENO)
        {

            matelo(cur);
            return;
        }
        else if (fd == STDOUT_FILENO)
        {
            // need to get the file for stdout
            matelo(cur);
            return;
        }
        else
        {
            struct file *target = cur->file_descriptor_table[fd];
            if (target != NULL)
            {
                lock_acquire(&file_lock);
                file_close(target);
                lock_release(&file_lock);
            }
        }

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
