#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys/directory.h"
#include "lib/kernel/hash.h"
#include "vm/page.h"
#include "threads/thread.h"

/*
 & INSERT DESCRIPTION

*/

static bool setup_stack(void **esp, int argc, char *argv[])
{

    return false;
}

/*
 & INSERT DESCRIPTION
 After physical memory alloction, load the file page from the disk to physical memory

*/
bool load_file(void *kaddr, struct Supplemental_Page_Table_Entry *spte)
{

    /* .
    ! Should be change it to load_page?
    * Using file_read_at()
    * Write physical memory as much as read_bytes by file_read_at
    *  Return file_read_at status
    *  Pad 0 as much as zero_bytes
    *  if file is loaded to memory, return true
    */
    return false;
}
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
 * Initialized an entry for a SPT entry with the virtual address kpage
 * Inseting into Hash Table of the current process
 */
void setup_spte(void *kpage)
{
    struct thread *curr = thread_current();

    struct Supplemental_Page_Table_Entry *spte = malloc(sizeof(struct Supplemental_Page_Table_Entry));
    spte->kaddr = NULL;                /* Kernel pages are 1-to-1 with frame? */
    spte->uaddr = kpage;               /* Passed in PAL_USER Flag into that */
    spte->status = DISK;               /* Maybe, cuz it has not been loaded yet */
    spte->dirty = false;               /* Still clean i guess */
    int hash_key = ((int)kpage) >> 12; /* Making page# the key for hash*/
    spte->key = hash_key;

    hash_insert(&curr->spt_hash, &spte->hash_elem);
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