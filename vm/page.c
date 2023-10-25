#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys/directory.h"
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

/**
 * @brief  Returns true if foo a precedes foo b.
 */
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux)
{
    const struct Supplemental_Page_Table_Entry *a = hash_entry(a_, struct Supplemental_Page_Table_Entry, hash_elem);
    const struct Supplemental_Page_Table_Entry *b = hash_entry(b_, struct Supplemental_Page_Table_Entry, hash_elem);

    return a->key < b->key;
}

/*
& INSERT DESCRIPTION
! Not Sure what for
*/

static bool setup_stack(void **esp, int argc, char *argv[])
{

    return false;
}

/*
 After physical memory alloction, load the file page from the disk to physical memory
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
            struct file *file = spte->file;
            if (file != NULL)
            {
                size_t page_read_bytes = spte->read_bytes < PGSIZE ? spte->read_bytes : PGSIZE;
                size_t page_zero_bytes = PGSIZE - page_read_bytes;
                off_t num = file_read_at(file, (void *)kaddr, page_read_bytes, spte->ofs);
                if ((int)num != (int)page_read_bytes)
                {
                    log(L_ERROR, "DID NOT READ %d BYTES FROM FILE, READ %d BYTES, PAIN \n", page_read_bytes, num);
                    // palloc_free_page(kpage);
                    return false;
                }
                // off_t bytes_read = file_read_at(file, kaddr, page_read_bytes, spte->ofs); /* maybe ideally */
                //  spte->current_file_pos = spte->ofs + page_read_bytes;
                memset(kaddr + page_read_bytes, 0, page_zero_bytes);
                return true;
            }
            log(L_ERROR, "ELF CASE, FILE WAS NULL \n");
            break;
        }
        case GENERAL:
        {

            break;
        }
        case ANONYMOUS:
        {
            break;
        }

        default:
            break;
        }
        break;
    }

    case SWAP:
    {
        /* SWAP LOGIC */
    }
    case MEMORY:
    {
        /* No need to do anything */
        break;
    }

    default:
        break;
    }

    return false;
}

/*
 * Initialized an entry for a SPT entry with the virtual address kpage
 * Inseting into Hash Table of the current process
 */
struct Supplemental_Page_Table_Entry *setup_spte(void *kpage)
{
    struct thread *curr = thread_current();

    struct Supplemental_Page_Table_Entry *spte = malloc(sizeof(struct Supplemental_Page_Table_Entry));
    spte->kaddr = NULL;                          /* Kernel pages are 1-to-1 with frame? */
    spte->uaddr = kpage;                         /* Passed in PAL_USER Flag into that */
    spte->location = DISK;                       /* Maybe, cuz it has not been loaded yet */
    spte->dirty = false;                         /* Still clean i guess */
    uint32_t hash_key = ((uint32_t)kpage) >> 12; /* Making page # (20 bits) the key for hash*/
    spte->key = hash_key;

    spte->file = NULL;
    log(L_TRACE, "Key goinging into HASH: [%04x] | UPAGE passed in setup_spte() [%04x]\n", spte->key, spte->uaddr);

    hash_insert(&curr->spt_hash, &spte->hash_elem);
    return spte;
}
struct Supplemental_Page_Table_Entry *setup_spte_from_file(void *upage, struct file *file, off_t ofs, bool writable, uint32_t read_bytes)
{
    struct thread *curr = thread_current();

    struct Supplemental_Page_Table_Entry *spte = malloc(sizeof(struct Supplemental_Page_Table_Entry));
    spte->kaddr = NULL;                          /* Kernel pages are 1-to-1 with frame? */
    spte->uaddr = upage;                         /* Passed in PAL_USER Flag into that */
    spte->location = DISK;                       /* Maybe, cuz it has not been loaded yet */
    spte->dirty = false;                         /* Still clean i guess */
    uint32_t hash_key = ((uint32_t)upage) >> 12; /* Making page # (20 bits) the key for hash*/
    spte->key = hash_key;
    spte->file = file;
    spte->source = ELF;
    spte->ofs = ofs;
    spte->writable = writable;
    spte->read_bytes = read_bytes;

    log(L_TRACE, "Key goinging into HASH: [%04x] | UPAGE passed in setup_spte_from_file() [%04x]\n", spte->key, spte->uaddr);
    hash_insert(&curr->spt_hash, &spte->hash_elem);
    return spte;
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

bool handle_mm_fault(struct Supplemental_Page_Table_Entry *spte)
{

    /* Get a page of memory. */

    log(L_TRACE, "KPAGE: [%08x] | UPAGE: [%08x] in handle_mm_fault\n", spte->kaddr, spte->uaddr);

    void *kpage = palloc_get_page(PAL_USER); /* Switched it to void */
    if (kpage == NULL)
    {
        log(L_ERROR, "Pain kpage is NULL \n");

        return false;
    }
    spte->kaddr = kpage;

    /* Would do the loading*/
    if (!load_page(spte->kaddr, spte))
    {
        log(L_ERROR, "Pain in load_page() \n");
        return false;
    }

    if (!install_page(spte->uaddr, spte->kaddr, spte->writable))
    {
        log(L_ERROR, "Pain in install_page()\n");
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
 * Given a Hash Table and an address (the key) it if found, func returns a Supplemental_Page_Table_Entry from HASH
 *  if not found, func returns NULL
 */
struct Supplemental_Page_Table_Entry *find_spte(struct hash hash, void *addr)
{
    struct Supplemental_Page_Table_Entry scratch;
    struct Supplemental_Page_Table_Entry *result = NULL;

    struct hash_elem *e;
    scratch.key = ((uint32_t)addr) >> 12;
    log(L_TRACE, "Key in Exeception.c:[%04x]\n", scratch.key);
    e = hash_find(&hash, &scratch.hash_elem);
    log(L_TRACE, "e:[%d]\n", e);
    if (e != NULL)
    {
        result = hash_entry(e, struct Supplemental_Page_Table_Entry, hash_elem);
        log(L_TRACE, "Key:[%08x] and Vaddr:[%08x]\n", result->key, result->uaddr);
        log(L_TRACE, "KPAGE: [%08x] | UPAGE: [%08x] in find_spte before handle_mm()\n", result->kaddr, result->uaddr);
    }
    return result;
}