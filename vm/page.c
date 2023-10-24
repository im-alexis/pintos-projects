#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys/directory.h"
#include "lib/kernel/hash.h"
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

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
bool load_file(void *kaddr, struct Supplemental_Page_Table_Entry *spte)
{
    /*
    ! Should be change it to load_page?
    * Using file_read_at()
    * Write physical memory as much as read_bytes by file_read_at
    *  Return file_read_at status
    *  Pad 0 as much as zero_bytes
    *  if file is loaded to memory, return true
    */
    if (spte->file_backed)
    {
        struct file *file = spte->file;
        if (file != NULL)
        {
            size_t page_read_bytes = spte->read_bytes < PGSIZE ? spte->read_bytes : PGSIZE;
            size_t page_zero_bytes = PGSIZE - page_read_bytes;
            off_t bytes_read = file_read_at(file, kaddr, page_read_bytes, spte->ofs); /* maybe ideally */
            spte->current_file_pos = spte->ofs + page_read_bytes;
            memset(kaddr + page_read_bytes, 0, page_zero_bytes);
            return true;
        }
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
    spte->status = DISK;                         /* Maybe, cuz it has not been loaded yet */
    spte->dirty = false;                         /* Still clean i guess */
    uint32_t hash_key = ((uint32_t)kpage) >> 12; /* Making page # (20 bits) the key for hash*/
    printf("Key goinging into HASH: [%04x]\n", hash_key);
    spte->key = hash_key;
    spte->file_backed = false;
    spte->file = NULL;

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
    load_file(spte->kaddr, spte); /* Would do the loading*/

    install_page(((uint8_t *)PHYS_BASE) - PGSIZE, spte->kaddr, spte->writable);
    spte->status = MEMORY;
    return false;
    /*
        When a page fault occurs, allocate physical memory
        Load file in the disk to physical moemory
        Use load_file(void* kaddr, struct vm_entry *vme)
        Update the associated poge table entry ater loading into physical memory
        Use static bool install_page(void *upage, void *kpage, bool writable)
    */
}