// #include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "lib/stdio.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"

#include "string.h"

static void syscall_handler(struct intr_frame *);
static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);
void close_thread_files();
void matelo();
bool valid_ptr(uint8_t *addy, uint8_t byte, int size, uint8_t type_of_call);
bool valid_ptr_v2(const void *addy);
bool check_buffer(void *buff_to_check, unsigned size);

static struct lock file_lock; // lock for synch when doing file related stuff

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int get_user(const uint8_t *uaddr)
{
    int result;
    asm("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a"(result) : "m"(*uaddr));
    return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool put_user(uint8_t *udst, uint8_t byte)
{
    int error_code;
    asm("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a"(error_code), "=m"(*udst) : "q"(byte));
    return error_code != -1;
}

void close_thread_files() /* -> it no worky :(  */
{
    struct thread *cur = thread_current;
    if (cur->how_many_fd == 2)
    {
        return;
    }
    for (int i = 2; i < MAX_FD; i++)
    {
        struct file *file = cur->file_descriptor_table[i];
        if (file != NULL)
        {
            lock_acquire(&file_lock);
            file_close(file);
            lock_release(&file_lock);
            cur->file_descriptor_table[i] = NULL;
            cur->how_many_fd--;
        }
        if (cur->how_many_fd == 2)
            break;
    }
}

/*
Terminates a thread with exit code -1
matelo (kill it)
*/
void matelo(struct thread *cur)
{
    // close_thread_files();
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

bool valid_ptr(uint8_t *addy, uint8_t byte, int size, uint8_t type_of_call)
{
    if (type_of_call == 1)
    {
        if (addy == NULL)
        {
            return false;
        }
        bool ret1 = put_user(addy, byte);
        bool ret2 = put_user(addy + (size - 1), byte);
        return (ret1 && ret2);
    }
    else if (type_of_call == 0)
    {
        if (addy == NULL)
        {
            return false;
        }
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

bool valid_ptr_v2(const void *addy)
{
    /* Check to see if the address is NULL, if it is valid for the user and that it is not below the start of virtual memory (0x08084000)  */
    if (!is_user_vaddr(addy) || addy == NULL || addy < (void *)0x08048000)
    {
        matelo(thread_current());
        return false;
    }
    return true;
}

bool check_buffer(void *buffer, unsigned size)
{
    char *pos = (char *)buffer;
    for (int i = 0; i < size; i++)
    {
        if (!valid_ptr_v2((const void *)pos))
        {
            return false;
        }
        pos++;
    }
    return true;
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
    lock_init(&file_lock);                 /* lock for any file related activity*/
    struct thread *cur = thread_current(); /*current thread calling a system call*/
    uint32_t *esp = f->esp;

    if (!valid_ptr_v2((const void *)esp)) /*Validates the Stack Pointer */
    {
        //matelo(cur);   
        return;
    }
    if(!(valid_ptr(esp, 0, 4, 0)))
    {
        matelo(cur);
        return;
    }
    
    uint32_t syscall_num = *esp;
    /*Getting the arguments from eso*/
    uint32_t *arg0 = esp + 1;
    uint32_t *arg1 = esp + 2;
    uint32_t *arg2 = esp + 3;

    if (*esp < SYS_HALT || *esp > SYS_INUMBER) // if there is a bad call number
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
        if (!valid_ptr_v2((const void *)arg0))
            return;
        int exit_code = ((int)*arg0);
        cur->exit_code = ((int)*arg0);
        // close_thread_files(); -> does not work, breaks everything
        thread_exit();
        break;
    }

    case SYS_EXEC:
    {
        if (!valid_ptr_v2((const void *)arg0))
            return;
        char *file_name = ((const char *)*arg0);
        lock_acquire(&file_lock);
        int val = process_execute(file_name);
        lock_release(&file_lock);
        f->eax = val;
        break;
    }

    case SYS_WAIT:
    {
        if (!valid_ptr_v2((const void *)arg0))
            return;
        tid_t tid = ((tid_t)*arg0);
        f->eax = process_wait(tid);
        break;
    }

    /*
    FILE RELATED SYSCALLS
    */
    case SYS_CREATE:
    {
        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1))
            return;

        char *file = ((const char *)*arg0);
        if (file == NULL)
        {
            matelo(cur);
            return;
        }
        unsigned size = ((unsigned)*arg1);
        lock_acquire(&file_lock);
        f->eax = filesys_create(file, size);
        lock_release(&file_lock);
        break;
    }
    case SYS_OPEN:
    {
        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1))
            return;

        char *file = ((const char *)*arg0);
        if (file == NULL)
        {
            matelo(cur);
            return;
        }
        lock_acquire(&file_lock);
        struct file *opened_file = filesys_open(file);
        lock_release(&file_lock);
        if (opened_file == NULL)
        {
            f->eax = -1;
            return;
        }
        int result = search_by_file(cur, opened_file);
        if (result != -1)
        {
            lock_acquire(&file_lock);
            file_close(file);
            lock_release(&file_lock);
            f->eax = -1;
            return;
        }
        else
        {
            f->eax = add_to_table(cur, opened_file);
            break;
        }
    }

    case SYS_REMOVE:
    {
        // search for a file in descriptor table
        if (!valid_ptr_v2((const void *)arg0))
            return;

        char *file = ((const char *)*arg0);
        lock_acquire(&file_lock);
        bool ret = removed_from_table_by_filename(file, cur);
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
        if (!valid_ptr_v2((const void *)arg0))
            return;
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
        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1) || !valid_ptr_v2((const void *)arg2))
            return;

        int fd = ((int)*arg0);
        char *buffer = ((char *)*arg1);
        off_t size = (off_t)*arg2;
        if (!check_buffer(*arg1, *arg2))
            return;

        if (fd == STDOUT_FILENO || fd >= MAX_FD) // check fd table is valid, make sure buffer is a valid pointer, check the size is greater than 0
        {
            matelo(cur);
            return;
        }
        /* Confused here. */
        else if (fd == STDIN_FILENO)
        {
            f->eax = input_getc(); // -> getis keyboard input ??
            break;
        }
        else
        {
            struct file *fdt = cur->file_descriptor_table[fd];

            if (fdt == NULL)
            {
                matelo(cur);
                return;
            }
            lock_acquire(&file_lock);
            file_deny_write(fdt);
            f->eax = file_read(fdt, buffer, size);
            lock_release(&file_lock);
            break;
        }

        break;
    }

    case SYS_WRITE: // validate with read check index [0] and [size-1] -> get_user()
    {

        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1) || !valid_ptr_v2((const void *)arg2))
            return;
        if (!check_buffer(*arg1, *arg2))
            return;

        int fd = ((int)*arg0);
        char *buffer = ((char *)*arg1);
        unsigned size = (unsigned)*arg2;

        bool result = valid_ptr(&buffer, -1, (int)size, 0);
        if (fd == STDIN_FILENO || fd >= MAX_FD || !result)
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
            struct file *exec_file = filesys_open(cur->executing_file);
            lock_release(&file_lock);
            //if()
            if (exec_file->inode == targeta->inode)
            {
                /* Why don't you also close targeta? */
                file_close(exec_file);
                cur->exit_code;
                f->eax = 0;
                return;
            }
            lock_acquire(&file_lock);
            file_close(exec_file);
            lock_release(&file_lock);

            lock_acquire(&file_lock);
            file_allow_write(targeta);
            if (!targeta->deny_write)
                f->eax = file_write(targeta, buffer, size);

            file_deny_write(targeta);
            lock_release(&file_lock);
        }

        break;
    }
    case SYS_SEEK: // file_seek()
    {
        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1))
            return;

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
        if (!valid_ptr_v2((const void *)arg0))
            return;
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
        if (!valid_ptr_v2((const void *)arg0))
            return;
        int fd = ((int)*arg0);
        if (fd == NULL || fd == STDOUT_FILENO || fd == STDIN_FILENO || fd >= MAX_FD)
        {
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
                removed_from_table(fd, cur);
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
}
