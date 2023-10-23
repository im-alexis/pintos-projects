#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lib/kernel/hash.h"
#include "vm/frame.h"
#include "filesys/directory.h"

// from Ed Discussion we declared the SPTE struct

enum page_location
{
    MEMORY,
    SWAP,
    DISK

    /*
    A.K.A MONSTER SIZED DICK ~~~
    */

    // the status in page.c will tell the frame (in a switch case) what direction to go: SWAP, MEMORY, DISK
};

struct Supplemental_Page_Table_Entry
{
    struct hash_elem hash_elem;
    /*

    *Location of each page: whether it’s in SWAP, MEMORY, or DISK  // does this mmean enum page_location? aka done?
    ! Uaddr and Kaddr (Done?? line 17 & 18)
    *Etc. (Don't know what this means)

    */
    void *uaddr; /* Key */
    void *kaddr;

    uint32_t key; /* Key */

    enum page_location status;
    bool dirty;
    bool file_backed;
};

bool load_file(void *kaddr, struct Supplemental_Page_Table_Entry *spte);
bool page_hash(const struct hash_elem *p_, void *aux);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
void setup_spte(void *kpage);
bool install_page(void *upage, void *kpage, bool writable);

#endif