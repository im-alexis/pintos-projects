#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/tss.h"
#include "lib/kernel/list.h"
#define LOGGING_LEVEL 6

#include <log.h>

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);

/* Starts a new thread running a user program loaded from
 * FILENAME.  The new thread may be scheduled (and may even exit)
 * before process_execute() returns.  Returns the new process's
 * thread id, or TID_ERROR if the thread cannot be created. */
tid_t process_execute(const char *file_name)
{
    char *fn_copy;
    char *file_name_cpy, *sav_ptr;
    tid_t tid;

    // NOTE:
    // To see this print, make sure LOGGING_LEVEL in this file is <= L_TRACE (6)
    // AND LOGGING_ENABLE = 1 in lib/log.h
    // Also, probably won't pass with logging enabled.
    log(L_TRACE, "Started process execute: %s", file_name);

    /* Make a copy of FILE_NAME.
     * Otherwise there's a race between the caller and load(). */
    fn_copy = palloc_get_page(0);
    if (fn_copy == NULL)
    {
        return TID_ERROR;
    }
    strlcpy(fn_copy, file_name, PGSIZE);

    file_name_cpy = malloc(strlen(file_name) + 1);
    if (file_name_cpy == NULL)
    {
        palloc_free_page(fn_copy);
        return TID_ERROR;
    }
    strlcpy(file_name_cpy, file_name, PGSIZE);
    file_name = strtok_r(file_name_cpy, " ", &sav_ptr); // getting that first token for file name

    /* Create a new thread to execute FILE_NAME. */
    tid = thread_create(file_name, PRI_DEFAULT, start_process, fn_copy);

    /*
    ADDS THE NEWLY CREATED THREAD TO THE PARENT mis_ninos list
    Notes: could be faster by just getting the last element in the list, but not sure if other threads can be created in the mean time.
    ie. t1 is created, t1 initialized, and in between t1 being initialized t2 could be created, messing the whole thing up
    */

    struct thread *cur = thread_current();
    struct thread *thread;

    struct list_elem *elm_in_list = list_begin(&cur->all_process_list); // set the entry for the list
    thread = list_entry(elm_in_list, struct thread, allelem);
    while (thread != NULL)
    {
        thread = list_entry(elm_in_list, struct thread, allelem); // calls the macro define in list
        if (thread->tid == tid)
            break;
        elm_in_list = list_next(&thread->allelem); // move to the next element
    }
    list_push_back(&cur->mis_ninos, &thread->chld_thrd_elm);
    thread->parent = cur;

    if (tid == TID_ERROR)
    {
        palloc_free_page(fn_copy);
    }
    sema_down(&cur->process_semma);
    return thread->tid;
}

/* A thread function that loads a user process and starts it
 * running. */
static void
start_process(void *file_name_)
{
    char *file_name = file_name_;
    struct intr_frame if_;
    bool success;

    log(L_TRACE, "start_process()");

    /* Initialize interrupt frame and load executable. */
    memset(&if_, 0, sizeof if_);
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;

    success = load(file_name, &if_.eip, &if_.esp); // load does a lot of stuff!
    /* If load failed, quit. */

    struct thread *cur = thread_current();
    struct thread *cur_parent = cur->parent;

    // palloc_free_page(file_name);
    // if (!success)
    // {
    //     cur->exit_code = -1;
    //     cur->tid = -1;
    //     thread_exit();
    // }

    if (success)
    {
        palloc_free_page(file_name);
        // struct file *ex = filesys_open(cur->executing_file);
        // file_deny_write(ex);
        sema_up(&cur_parent->process_semma);
    }
    else
    {
        palloc_free_page(file_name);
        // cur->exit_code = -1;
        cur->tid = -1;
        sema_up(&cur_parent->process_semma);
        thread_exit();
    }

    /* Start the user process by simulating a return from an
     * interrupt, implemented by intr_exit (in
     * threads/intr-stubs.S).  Because intr_exit takes all of its
     * arguments on the stack in the form of a `struct intr_frame',
     * we just point the stack pointer (%esp) to our stack frame
     * and jump to it. */

    // hex_dump(if_.esp, if_.esp, PHYS_BASE - if_.esp); // checking if the stack was set up correctly

    asm volatile("movl %0, %%esp; jmp intr_exit"
                 :
                 : "g"(&if_)
                 : "memory");
    NOT_REACHED();
}
/*
This fumction is very experimental, not really sure how the list works

*/
bool is_my_child(tid_t child_tid);
bool is_my_child(tid_t child_tid)
{
    struct thread *cur = thread_current();
    struct thread *child_thread;
    bool found = false;

    /* Makes use of the list struct in kernel */
    struct list_elem *child_in_list = list_begin(&cur->mis_ninos); // set the entry for the list
    child_thread = list_entry(child_in_list, struct thread, chld_thrd_elm);

    while (child_thread != NULL)
    {
        child_thread = list_entry(child_in_list, struct thread, chld_thrd_elm);
        if (!is_thread(child_thread))
        {
            break;
        }

        if (child_thread->tid == child_tid)
        {
            return true;
        }
        child_in_list = list_next(&child_thread->chld_thrd_elm);
    }
    return false;
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int process_wait(tid_t child_tid UNUSED)
{
    struct thread *thread_waited_on = find_thread_by_tid(child_tid);

    if (!is_my_child(child_tid) || thread_waited_on == NULL)
    {
        return -1;
    }

    if (!(thread_waited_on->has_been_waited_on))
    {
        thread_waited_on->has_been_waited_on = true;
        sema_down(&thread_waited_on->exiting_thread); // wait for the child process to finish
        int exit_code = thread_waited_on->exit_code;
        sema_up(&thread_waited_on->reading_exit_status);

        return exit_code;
    }
    return -1;
    // 3. needs to properly wait
    // suma down suma up
}

/* Free the current process's resources. */
void process_exit(void)
{
    struct thread *cur = thread_current();
    uint32_t *pd;
    printf("%s: exit(%d)\n", cur->name, cur->exit_code);

    /* Destroy the current process's page directory and switch back
     * to the kernel-only page directory. */
    pd = cur->pagedir;

    if (pd != NULL)
    {
        /* Correct ordering here is crucial.  We must set
         * cur->pagedir to NULL before switching page directories,
         * so that a timer interrupt can't switch back to the
         * process page directory.  We must activate the base page
         * directory before destroying the process's page
         * directory, or our active page directory will be one
         * that's been freed (and cleared). */

        // cur->exit_code = 0;
        sema_up(&cur->exiting_thread);
        sema_down(&cur->reading_exit_status);
        cur->pagedir = NULL;
        pagedir_activate(NULL);
        pagedir_destroy(pd);
        return;
    }
    sema_up(&cur->exiting_thread);
    sema_down(&cur->reading_exit_status);
}

/* Sets up the CPU for running user code in the current
 * thread.
 * This function is called on every context switch. */
void process_activate(void)
{
    struct thread *t = thread_current();

    /* Activate thread's page tables. */
    pagedir_activate(t->pagedir);

    /* Set thread's kernel stack for use in processing
     * interrupts. */
    tss_update();
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
    unsigned char e_ident[16];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
 * There are e_phnum of these, starting at file offset e_phoff
 * (see [ELF1] 1-6). */
struct Elf32_Phdr
{
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

static bool setup_stack(void **esp, int argc, char *argv[]);

static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *EIP
 * and its initial stack pointer into *ESP.
 * Returns true if successful, false otherwise. */
bool load(const char *file_name, void (**eip)(void), void **esp)
{
    struct lock *file_lock;
    lock_init(&file_lock);
    log(L_TRACE, "load()");
    struct thread *t = thread_current();
    struct Elf32_Ehdr ehdr;
    struct file *file = NULL;
    off_t file_ofs;
    bool success = false;
    int i;

    /* Allocate and activate page directory. */
    t->pagedir = pagedir_create();
    if (t->pagedir == NULL)
    {
        goto done;
    }
    process_activate();

    char *sav_ptr, *token;
    char *args[24];
    int counter = 0;
    sav_ptr = file_name;
    token = strtok_r(sav_ptr, " ", &sav_ptr);
    args[counter] = token;
    counter++;

    while (token = strtok_r(NULL, " ", &sav_ptr))
    {
        args[counter] = token;
        counter++;
    }

    /* Open executable file. */
    lock_acquire(&file_lock);
    file = filesys_open(args[0]);
    if (file != NULL)
    {
        file_deny_write(file);
    }
    lock_release(&file_lock);
    if (file == NULL)
    {
        // t->exit_code = 0;
        // t->tid = -1;
        printf("load: %s: open failed\n", args[0]);
        goto done;
    }

    /* Read and verify executable header. */
    if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 3 || ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024)
    {
        printf("load: %s: error loading executable\n", args[0]);
        goto done;
    }

    /* Read program headers. */
    file_ofs = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++)
    {
        struct Elf32_Phdr phdr;

        if (file_ofs < 0 || file_ofs > file_length(file))
        {
            goto done;
        }
        file_seek(file, file_ofs);

        if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
        {
            goto done;
        }
        file_ofs += sizeof phdr;
        switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
            /* Ignore this segment. */
            break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
            goto done;
        case PT_LOAD:
            if (validate_segment(&phdr, file))
            {
                bool writable = (phdr.p_flags & PF_W) != 0;
                uint32_t file_page = phdr.p_offset & ~PGMASK;
                uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
                uint32_t page_offset = phdr.p_vaddr & PGMASK;
                uint32_t read_bytes, zero_bytes;
                if (phdr.p_filesz > 0)
                {
                    /* Normal segment.
                     * Read initial part from disk and zero the rest. */
                    read_bytes = page_offset + phdr.p_filesz;
                    zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
                }
                else
                {
                    /* Entirely zero.
                     * Don't read anything from disk. */
                    read_bytes = 0;
                    zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
                }
                if (!load_segment(file, file_page, (void *)mem_page,
                                  read_bytes, zero_bytes, writable))
                {
                    goto done;
                }
            }
            else
            {
                goto done;
            }
            break;
        }
    }

    /* Set up stack. */
    if (!setup_stack(esp, counter, args))
    {
        goto done;
    }

    /* Start address. */
    *eip = (void (*)(void))ehdr.e_entry;

    success = true;

done:
    /* We arrive here whether the load is successful or not. */
    file_close(file);
    return success;
}

/* load() helpers. */

static bool install_page(void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment(const struct Elf32_Phdr *phdr, struct file *file)
{
    /* p_offset and p_vaddr must have the same page offset. */
    if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    {
        return false;
    }

    /* p_offset must point within FILE. */
    if (phdr->p_offset > (Elf32_Off)file_length(file))
    {
        return false;
    }

    /* p_memsz must be at least as big as p_filesz. */
    if (phdr->p_memsz < phdr->p_filesz)
    {
        return false;
    }

    /* The segment must not be empty. */
    if (phdr->p_memsz == 0)
    {
        return false;
    }

    /* The virtual memory region must both start and end within the
     * user address space range. */
    if (!is_user_vaddr((void *)phdr->p_vaddr))
    {
        return false;
    }
    if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
    {
        return false;
    }

    /* The region cannot "wrap around" across the kernel virtual
     * address space. */
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    {
        return false;
    }

    /* Disallow mapping page 0.
     * Not only is it a bad idea to map page 0, but if we allowed
     * it then user code that passed a null pointer to system calls
     * could quite likely panic the kernel by way of null pointer
     * assertions in memcpy(), etc. */
    if (phdr->p_vaddr < PGSIZE)
    {
        return false;
    }

    /* It's okay. */
    return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 *      - READ_BYTES bytes at UPAGE must be read from FILE
 *        starting at offset OFS.
 *
 *      - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
             uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    log(L_TRACE, "load_segment()");

    file_seek(file, ofs);
    while (read_bytes > 0 || zero_bytes > 0)
    {
        /* Calculate how to fill this page.
         * We will read PAGE_READ_BYTES bytes from FILE
         * and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* Get a page of memory. */
        uint8_t *kpage = palloc_get_page(PAL_USER);
        if (kpage == NULL)
        {
            return false;
        }

        /*
        ! Delete this section
        */

        /* Load this page. */
        if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
        {
            palloc_free_page(kpage);
            return false;
        }
        memset(kpage + page_read_bytes, 0, page_zero_bytes);

        /* Add the page to the process's address space. */
        if (!install_page(upage, kpage, writable))
        {
            palloc_free_page(kpage);
            return false;
        }
        /*
        ! Delete this section
        */

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
    }
    return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
 * user virtual memory. */
static bool
setup_stack(void **esp, int argc, char *argv[])
{

    uint8_t *kpage;
    bool success = false;

    log(L_TRACE, "setup_stack()");

    kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage != NULL)
    {
        success = install_page(((uint8_t *)PHYS_BASE) - PGSIZE, kpage, true);
        if (success)

        {
            /* Offset PHYS_BASE as instructed. */
            //*esp = PHYS_BASE - 12;
            *esp = PHYS_BASE;

            /* A list of addresses to the values that are initially added to the stack.  */
            uint32_t *arg_val_ptr[argc];
            uint32_t byte_count = 0;

            /* First add all of the command line arguments in descending order, including
               the program name. */
            for (int i = argc - 1; i >= 0; i--)
            {
                /* Allocate enough space for the entire string (plus an extra byte for
                   '/0'). Copy the string to the stack, and add its reference to the array
                    of pointers. Keep track of how many bytes where needed for the arguments */

                byte_count = byte_count + sizeof(char) * (strlen(argv[i]) + 1);

                *esp = *esp - sizeof(char) * (strlen(argv[i]) + 1);
                memcpy(*esp, argv[i], sizeof(char) * (strlen(argv[i]) + 1));
                arg_val_ptr[i] = (uint32_t *)*esp;
            }

            // This padding for 4 byte allingment
            uint32_t padding_need = byte_count % 4;
            for (int i = 0; i < padding_need; i++)
            {
                *esp = *esp - sizeof(char);
                (*(char *)(*esp)) = 0;
            }

            // Allocate space for & add the null sentinel.
            *esp = *esp - 4;
            (*(int *)(*esp)) = 0;

            /* Push onto the stack each char* in arg_value_pointers[] (each of which
               references an argument that was previously added to the stack). */
            *esp = *esp - 4;
            for (int i = argc - 1; i >= 0; i--)
            {
                (*(uint32_t **)(*esp)) = arg_val_ptr[i];
                *esp = *esp - 4;
            }

            /* Push onto the stack a pointer to the pointer of the address of the
               first argument in the list of arguments. */
            (*(uintptr_t **)(*esp)) = *esp + 4;

            *esp = *esp - 4;
            *(int *)(*esp) = argc; // put how many arguments there are

            // Push onto the stack a fake return address
            *esp = *esp - 4;
            (*(int *)(*esp)) = 0; // put the return address at the end

            /*USER STACK SHOULD BE SETUP BY NOW*/
        }

        else
        {
            palloc_free_page(kpage);
        }
        // hex_dump(*(int *)esp, *esp, 128, true); // NOTE: uncomment this to check arg passing
    }

    /* Create vm entry*/
    /* Set up vm_entry*/
    /* Using insert_vme(), add vm_entry to hash table*/
    return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     * address, then map our page there. */
    return pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable);
}

/*
? Create
bool handle_mm_fault(struct Supplemental_Page_Table *spt){

    When a page fault occurs, allocate physical memory
    Load file in the disk to physical moemory
        Use load_file(void* kaddr, struct vm_entry *vme)
    Update the associated poge table entry ater loading into physical memory
        Use static bool install_page()
}
*/