#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "lib/kernel/hash.h"
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#define LOGGING_LEVEL 6

#include <log.h>

bool page_hash(const struct hash_elem *p_, void *aux)
{
    const struct Supplemental_Page_Table_Entry *p = hash_entry(p_, struct Supplemental_Page_Table_Entry, hash_elem);
    return hash_bytes(&p->key, sizeof(p->key));
}

/*
 & Returns true if foo a precedes foo b.
 */
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux)
{
    const struct Supplemental_Page_Table_Entry *a = hash_entry(a_, struct Supplemental_Page_Table_Entry, hash_elem);
    const struct Supplemental_Page_Table_Entry *b = hash_entry(b_, struct Supplemental_Page_Table_Entry, hash_elem);

    return a->key < b->key;
}

/*

  & Another version of setup stack that creates an spte entry and allocates a frame
  & It takes in the same params as original (SETUP_STACK)
*/

static bool setup_stack_page(void **esp, int argc, char *argv[])
{
    struct Supplemental_Page_Table_Entry *spte = setup_spte_general(*esp);
    if (handle_mm_fault(*esp))
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
        struct thread *cur = thread_current();
        cur->stack_pointer = esp;
        return true;
    }

    return false;
}

/*
& After physical memory alloction, load the file page from the disk to physical memory
*/
bool load_page(void *kaddr, struct Supplemental_Page_Table_Entry *spte)
{
    /*
     * Using file_read_at()
     * Write physical memory as much as read_bytes by file_read_at
     *  Return file_read_at status
     *  Pad 0 as much as zero_bytes
     *  if file is loaded to memory, return true
     */
    switch (spte->location)
    {
    case DISK:
    { /* ANOTHER SWITCH TO LOAD FROM FROM FILE OR SOME WHERE ELSE */

        switch (spte->source)
        {
        case ELF:
        {
            if (spte->file != NULL)
            {
                struct file *file = spte->file;
                size_t page_read_bytes = spte->read_bytes < PGSIZE ? spte->read_bytes : PGSIZE;
                size_t page_zero_bytes = PGSIZE - page_read_bytes;
                off_t num = file_read_at(spte->file, spte->kaddr, page_read_bytes, spte->ofs);
                // file_seek(spte->file, spte->ofs);
                //  off_t num = file_read(spte->file, kaddr, spte->read_bytes);

                if ((int)num != (int)page_read_bytes)
                {
                    log(L_ERROR, "DID NOT READ %d BYTES FROM FILE, READ %d BYTES, PAIN", page_read_bytes, num);
                    // palloc_free_page(kpage);
                    return false;
                }
                log(L_DEBUG, "DID  READ %d BYTES FROM FILE, YAY", page_read_bytes, num);
                memset(kaddr + page_read_bytes, 0, page_zero_bytes);
                return true;
            }
            log(L_ERROR, "ELF CASE, FILE WAS NULL");
            break;
        }
        case GENERAL: /* I ASSUME THEY NEED TO BE ALL ZEROs*/
        {

            // struct file *file = spte->file;
            size_t page_read_bytes = spte->read_bytes < PGSIZE ? spte->read_bytes : PGSIZE;
            size_t page_zero_bytes = PGSIZE - page_read_bytes;
            // off_t num = file_read_at(spte->file, spte->kaddr, page_read_bytes, spte->ofs);
            //  file_seek(spte->file, spte->ofs);
            //   off_t num = file_read(spte->file, kaddr, spte->read_bytes);

            // if ((int)num != (int)page_read_bytes)
            // {
            //     log(L_ERROR, "DID NOT READ %d BYTES FROM FILE, READ %d BYTES, PAIN", page_read_bytes, num);
            //     // palloc_free_page(kpage);
            //     return false;
            // }
            // log(L_DEBUG, "DID  READ %d BYTES FROM FILE, YAY", page_read_bytes, num);
            memset(kaddr, 0, PGSIZE);
            return true;

            break;
        }
            /*
            only need to
            */
            // case ANONYMOUS:
            // {
            //     /*
            //     TODO this is for
            //     */
            //     break;
            // }

        default:
            break;
        }
        break;
    }

    case SWAP:
    {
        /*
        TODO Implement Swap logic
        */
        /* SWAP LOGIC */
    }
    case MEMORY:
    {
        /* No need to do anything */
        return true;
        break;
    }

    default:
        break;
    }

    return false;
}

/*
 & Initialized an entry for a SPT entry with the virtual address kpage
 & Inseting into Hash Table of the current process
 */
struct Supplemental_Page_Table_Entry *setup_spte_general(void *addr)
{
    struct thread *curr = thread_current();

    struct Supplemental_Page_Table_Entry *spte = malloc(sizeof(struct Supplemental_Page_Table_Entry));
    spte->kaddr = NULL;                         /* Kernel pages are 1-to-1 with frame? */
    spte->uaddr = addr;                         /* Passed in PAL_USER Flag into that */
    spte->location = DISK;                      /* Maybe, cuz it has not been loaded yet */
    spte->dirty = false;                        /* Still clean i guess */
    uint32_t hash_key = ((uint32_t)addr) >> 12; /* Making page # (20 bits) the key for hash*/
    spte->key = hash_key;
    spte->source = GENERAL;
    spte->file = NULL;
    log(L_INFO, "Key goinging into HASH: [%04x] | UPAGE passed in setup_spte_general() [%04x]", spte->key, spte->uaddr);

    hash_insert(&curr->spt_hash, &spte->hash_elem);
    return spte;
}

/*
 & Initialized an entry for a SPT entry with the virtual address kpage
 & Inseting into Hash Table of the current process
 */
struct Supplemental_Page_Table_Entry *setup_spte_from_file(void *upage, struct file *file, off_t ofs, bool writable, uint32_t read_bytes)
{
    struct thread *curr = thread_current();

    struct Supplemental_Page_Table_Entry *spte = (struct supplemental_page_table_entry *)malloc(sizeof(struct Supplemental_Page_Table_Entry));
    spte->kaddr = NULL;  /* Kernel pages are 1-to-1 with frame? */
    spte->uaddr = upage; /* Passed in PAL_USER Flag into that */
    spte->location = DISK;
    /* Maybe, cuz it has not been loaded yet */
    spte->dirty = false;                         /* Still clean i guess */
    uint32_t hash_key = ((uint32_t)upage) >> 12; /* Making page # (20 bits) the key for hash*/
    spte->key = hash_key;
    spte->file = file;

    spte->read_bytes = read_bytes;

    log(L_DEBUG, "Key goinging into HASH: [%04x] | UPAGE passed in setup_spte_from_file() [%04x]", spte->key, spte->uaddr);

    spte->source = ELF;
    spte->ofs = ofs;
    spte->writable = writable;
    hash_insert(&curr->spt_hash, &spte->hash_elem);
    return spte;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 & virtual address KPAGE to the page table.
 & If WRITABLE is true, the user process may modify the page;
 & otherwise, it is read-only.
 & UPAGE must not already be mapped.
 & KPAGE should probably be a page obtained from the user pool
 & with palloc_get_page().
 & Returns true on success, false if UPAGE is already mapped or
 & if memory allocation fails. */
bool install_page(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     * address, then map our page there. */
    return pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable);
}

/*
 Desciption
*/

bool handle_mm_fault(void *addr)
{

    /* Get a page of memory. */

    struct thread *cur = thread_current();
    struct Supplemental_Page_Table_Entry *spte = find_spte(cur->spt_hash, addr);
    if (spte == NULL)
    {
        log(L_INFO, "ADDR: [%08x] is not part of this thread", addr);
        return false;
    }
    // log(L_TRACE, "KPAGE: [%08x] | UPAGE: [%08x] in handle_mm_fault\n", spte->kaddr, spte->uaddr);
    void *kpage = palloc_get_page(PAL_USER); /* Switched it to void */
    if (kpage == NULL)
    {
        log(L_ERROR, "Pain kpage is NULL");

        return false;
    }
    spte->kaddr = kpage;
    // log(L_TRACE, "KPAGE: [%08x] | UPAGE: [%08x] in handle_mm_fault\n", spte->kaddr, spte->uaddr);

    /* Would do the loading*/
    if (!load_page(spte->kaddr, spte))
    {
        log(L_ERROR, "Pain in load_page()");
        return false;
    }

    if (!install_page(spte->uaddr, spte->kaddr, spte->writable))
    {
        log(L_ERROR, "Pain in install_page()");
        return false;
    }
    spte->location = MEMORY;
    return true;
    /*
        When a page fault occurs, allocate physical memory
        Load file in the disk to physical moemory
        Use load_page(void* kaddr, struct vm_entry *vme)
        Update the associated poge table entry ater loading into physical memory
        Use static bool install_page(void *upage, void *kpage, bool writable)
    */
}
/*
 & Given a Hash Table and an address (the key) it if found, func returns a Supplemental_Page_Table_Entry from HASH
 &  if not found, func returns NULL
 */
struct Supplemental_Page_Table_Entry *find_spte(struct hash hash, void *addr)
{
    struct Supplemental_Page_Table_Entry scratch;
    struct Supplemental_Page_Table_Entry *result = NULL;

    struct hash_elem *e;
    scratch.key = ((uint32_t)addr) >> 12;
    // og(L_TRACE, "Key in Exeception.c:[%04x]\n", scratch.key);
    e = hash_find(&hash, &scratch.hash_elem);
    // log(L_TRACE, "e:[%d]\n", e);
    if (e != NULL)
    {
        result = hash_entry(e, struct Supplemental_Page_Table_Entry, hash_elem);
        log(L_DEBUG, "KPAGE: [%08x] | UPAGE: [%08x] | Key:[%08x] in find_spte before handle_mm()", result->kaddr, result->uaddr, result->key);
    }
    return result;
}