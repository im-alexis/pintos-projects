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
#define LOGGING_LEVEL 6
#include <log.h>

static void syscall_handler(struct intr_frame *);
static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);

void matelo();
bool valid_ptr(uint8_t *addy, uint8_t byte, int size, uint8_t type_of_call);
bool valid_ptr_v2(const void *addy);
bool check_buffer(void *buff_to_check, unsigned size);

static struct lock file_lock; // lock for synch when doing file related stuff

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&file_lock);
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

/*
Terminates a thread with exit code -1
matelo (kill it)
*/
void matelo()
{
    log(L_TRACE, "matelo()");
    struct thread *cur = thread_current();
    close_thread_files();
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
    // log(L_TRACE, "Address being validated in valid_ptr():[%08x]\n", addy);
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
    // log(L_TRACE, "Address being validated in valid_ptr_v2():[%08x]\n", addy);
    /* Check to see if the address is NULL, if it is valid for the user and that it is not below the start of virtual memory (0x08084000)  */
    if (!is_user_vaddr(addy) || addy == NULL || addy < (void *)0x08048000)
    {
        matelo();
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
    log(L_TRACE, "syscall_handler()");
    /* lock for any file related activity*/
    struct thread *cur = thread_current(); /*current thread calling a system call*/
    uint32_t *esp = f->esp;
    cur->stack_pointer = f->esp;

    if (!valid_ptr_v2((const void *)esp)) /*Validates the Stack Pointer */
    {
        return;
    }
    if (!(valid_ptr(esp, 0, 4, 0)))
    {
        matelo();
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
    log(L_INFO, "Syscall number [%d]", syscall_num);

    switch (syscall_num)
    {
        /*
        PROCESS RELATED SYSCALLS
        */

    case SYS_HALT:
    {
        log(L_TRACE, "SYS_HALT");
        f->eax = shutdown_power_off();
        break;
    }

    case SYS_EXIT:
    {
        log(L_TRACE, "SYS_EXIT");
        if (!valid_ptr_v2((const void *)arg0))
            return;
        int exit_code = ((int)*arg0);
        cur->exit_code = exit_code;
        // close_thread_files();
        thread_exit();
        break;
    }

    case SYS_EXEC:
    {
        log(L_TRACE, "SYS_EXEC");
        if (!valid_ptr_v2((const void *)arg0))
            return;
        char *file_name = ((const char *)*arg0);
        int val = process_execute(file_name);
        f->eax = val;
        break;
    }

    case SYS_WAIT:
    {
        log(L_TRACE, "SYS_WAIT");
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
            matelo();
            return;
        }
        unsigned size = ((unsigned)*arg1);
        log(L_TRACE, "SYS_CREATE (file: [%s], size: [%d])", file, size);

        f->eax = filesys_create(file, size, false);

        break;
    }
    case SYS_OPEN:
    {
        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1))
            return;

        char *file = ((const char *)*arg0);
        if (!strcmp(file, ""))
        {
            f->eax = -1;
            return;
        }
        log(L_TRACE, "SYS_OPEN(file: [%s])", file);

        struct file *opened_file = filesys_open(file);

        if (opened_file == NULL)
        {
            log(L_ERROR, "Could not open the file: [%s]", file);
            f->eax = -1;
            return;
        }
        struct file_plus *pfile = create_file_plus(opened_file, file);
        if (pfile == NULL)
        {
            log(L_ERROR, "Could not create a pfile");
            destroy_plus_file(pfile, true);
            f->eax = -1;
            return;
        }
        else
        {
            log(L_INFO, "File: [%s] was successfully created", file);
            f->eax = add_to_table_plus(pfile);
            break;
        }
    }

    case SYS_REMOVE:
    {

        if (!valid_ptr_v2((const void *)arg0))
            return;

        char *name = ((const char *)*arg0);
        log(L_TRACE, "SYS_REMOVE(file_name: [%s])", name);
        f->eax = filesys_remove(name);

        break;
    }
        // idk what we are doing, we are drunk, sober alexis, look here plz
        // javier approved of this one,,

    case SYS_FILESIZE: // file_length();
    {
        if (!valid_ptr_v2((const void *)arg0))
            return;
        int fd = ((int)*arg0);
        log(L_TRACE, "SYS_FILESIZE(fd: [%d])", fd);
        if ((fd != STDIN_FILENO) && (fd != STDOUT_FILENO))
        {
            struct file_plus *t = cur->file_descriptor_table_plus[fd];
            struct file *target = t->file;
            if (target != NULL)
            {
                lock_acquire(&file_lock);
                int val = file_length(target);
                f->eax = val;
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
        log(L_TRACE, "SYS_READ(fd: [%d], size: [%d])", fd, size);
        if (fd == STDOUT_FILENO || fd >= MAX_FD) // check fd table is valid, make sure buffer is a valid pointer, check the size is greater than 0
        {
            matelo();
            return;
        }
        else if (fd == STDIN_FILENO)
        {
            f->eax = input_getc(); // -> getis keyboard input ??
            break;
        }
        else
        {
            struct file_plus *t = cur->file_descriptor_table_plus[fd];
            struct file *fdt = t->file;

            if (fdt == NULL)
            {
                matelo();
                return;
            }
            f->eax = file_read(fdt, buffer, size);
            break;
        }

        break;
    }

    case SYS_WRITE: // validate with read check index [0] and [size-1] -> get_user()
    {
        log(L_TRACE, "SYS_WRITE");
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
            log(L_ERROR, "Tried to write STDIN");
            f->eax = 0;
            matelo();
            return;
        }
        else if (fd == STDOUT_FILENO)
        {
            putbuf(buffer, size); // writes to the console
            f->eax = size;
        }
        else
        {
            struct file_plus *t = cur->file_descriptor_table_plus[fd];
            struct file *target = t->file;
            if (target == NULL)
            {
                f->eax = 0;
                log(L_ERROR, "File is empty");
                matelo();
                return;
            }
            char *cur_exec_filename = cur->executing_file;
            char *parent_exec_filename = cur->parent->executing_file;
            // log(L_DEBUG, "cur_exec: [%s] | parent_exec: [%s] | filename [%s]", cur_exec_filename, parent_exec_filename, t->name);
            // log(L_DEBUG, "Comp1 Result: [%d] | Comp2 Result: [%d]", strcmp(cur_exec_filename, t->name), strcmp(parent_exec_filename, t->name));
            if (!strcmp(cur_exec_filename, t->name) || !strcmp(parent_exec_filename, t->name))
            {
                log(L_ERROR, "Tried to write to ELF file");
                f->eax = 0;
                return;
            }
            if (target->inode->data.isDir)
            {
                log(L_ERROR, "Tried to write to directory");
                f->eax = 0;
                matelo();
                break;
            }
            f->eax = file_write(target, buffer, size);
        }

        break;
    }
    case SYS_SEEK: // file_seek()
    {

        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1))
            return;

        int fd = ((int)*arg0);
        unsigned size = (unsigned)*arg1;
        log(L_TRACE, "SYS_SEEK(fd:[%d], pos:[%d])", fd, size);
        if (fd == STDIN_FILENO || fd == STDOUT_FILENO)
        {
            break;
        }
        else
        {
            struct file_plus *t = cur->file_descriptor_table_plus[fd];
            struct file *target = t->file;

            if (target != NULL)
            {
                file_seek(target, size);
            }
        }

        break;
    }

    case SYS_TELL: // file_tell();
    {
        if (!valid_ptr_v2((const void *)arg0))
            return;
        int fd = ((int)*arg0);
        log(L_TRACE, "SYS_TELL(fd: [%d])", fd);
        if (fd == STDIN_FILENO || fd == STDOUT_FILENO)
        {
            break;
        }
        else
        {
            struct file_plus *t = cur->file_descriptor_table_plus[fd];
            struct file *target = t->file;
            if (target != NULL)
            {
                f->eax = file_tell(target);
            }
            else
            {
                matelo();
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
        log(L_TRACE, "SYS_CLOSE(fd: [%d])", fd);
        if (fd == NULL || fd == STDOUT_FILENO || fd == STDIN_FILENO || fd >= MAX_FD)
        {
            matelo();
            return;
        }
        else
        {
            struct file_plus *t = cur->file_descriptor_table_plus[fd];
            struct file *target = NULL;
            if (t != NULL)
            {
                target = t->file;
            }

            if (target != NULL)
            {
                file_close(target);
                removed_from_table_plus(fd);
            }
        }

        break;
    }
    case SYS_CHDIR:
    {
        /* This could be the only place to change directory */
        if (!valid_ptr_v2((const void *)arg0))
            return;
        char *dir = ((char *)*arg0);
        log(L_TRACE, "SYS_CHDIR(dir: [\"%s\"])", dir);
        if (dir == NULL)
        {
            matelo();
            return;
        }

        f->eax = filesys_chdir(dir); /* Function is a work in progress*/
        break;
    }
    case SYS_MKDIR:
    {
        if (!valid_ptr_v2((const void *)arg0))
            return;
        char *dir = ((char *)*arg0);
        log(L_TRACE, "SYS_MKDIR(dir: [\"%s\"])", dir);
        if (dir == NULL)
        {
            matelo();
            return;
        }

        if (!strcmp(dir, ""))
        {
            log(L_DEBUG, "Directory Path is empty");
            f->eax = false;
        }

        f->eax = filesys_create(dir, 0, true);
        break;
    }
    case SYS_READDIR:
    {
        log(L_TRACE, "SYS_READDIR");
        if (!valid_ptr_v2((const void *)arg0) || !valid_ptr_v2((const void *)arg1))
            return;
        int fd = ((int)*arg0);
        char *name = ((char *)*arg1);
        if (name == NULL)
        {
            matelo();
            return;
        }
        log(L_TRACE, "SYS_READDIR(fd: [%d), name: [%s]", fd, name);
        f->eax = false;
        if (fd >= MAX_FD || fd < 0)
        {
            matelo();
            break;
        }
        struct file_plus *t = cur->file_descriptor_table_plus[fd];
        if (t == NULL)
        {
            matelo();
            break;
        }
        struct file *target = t->file;
        if (target == NULL)
        {
            matelo();
            break;
        }
        if (target->inode->data.isDir)
        {
            struct dir *dir = (struct dir *)target;
            f->eax = dir_readdir(dir, name);
        }

        break;
    }

    case SYS_ISDIR:
    {
        if (!valid_ptr_v2((const void *)arg0))
            return;
        int fd = ((int)*arg0);
        log(L_TRACE, "SYS_ISDIR(fd: [%d])", fd);
        if (fd >= MAX_FD || fd < 0)
        {
            matelo();
            break;
        }
        struct file_plus *t = cur->file_descriptor_table_plus[fd];
        if (t == NULL)
        {
            matelo();
            break;
        }
        struct file *target = t->file;
        if (target == NULL)
        {
            matelo();
            break;
        }

        f->eax = target->inode->data.isDir;
        break;
    }
    case SYS_INUMBER:
    {
        if (!valid_ptr_v2((const void *)arg0))
            return;
        int fd = ((int)*arg0);
        log(L_TRACE, "SYS_INUMBER(fd: [%d])", fd);
        if (fd >= MAX_FD || fd < 0)
        {
            matelo();
            break;
        }
        struct file_plus *t = cur->file_descriptor_table_plus[fd];
        if (t == NULL)
        {
            matelo();
            break;
        }
        struct file *target = t->file;
        if (target == NULL)
        {
            matelo();
            break;
        }
        f->eax = target->inode;
        break;
    }
    default:
        break;
    }
}