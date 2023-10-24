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
#include "filesys/file.h"

// from Ed Discussion we declared the SPTE struct

enum page_location
{
    MEMORY,
    SWAP,
    DISK
    // the status in page.c will tell the frame (in a switch case) what direction to go: SWAP, MEMORY, DISK
};
enum Source
{
    ELF,     /* File Backed */
    General, /* STACK or Variables?? */
    Anymous
    // the status in page.c will tell the frame (in a switch case) what direction to go: SWAP, MEMORY, DISK
};

struct Supplemental_Page_Table_Entry
{
    struct hash_elem hash_elem;
    uint32_t key; /* Key */
    /*
    *Location of each page: whether it’s in SWAP, MEMORY, or DISK  // does this mmean enum page_location? aka done?
    ! Uaddr and Kaddr (Done?? line 17 & 18)
    *Etc. (Don't know what this means)
    */
    void *uaddr;               /* User Address - Virtual Page */
    void *kaddr;               /* Kernel Address - Frame ?? */
    enum page_location status; /* Where it currently is */
    bool dirty;                /* Was it written to */
    bool file_backed;
    struct file *file;

    /*
    & APEMAN MONEUVERS
    */
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
    off_t current_file_pos; /* */
};

bool load_file(void *kaddr, struct Supplemental_Page_Table_Entry *spte);
bool page_hash(const struct hash_elem *p_, void *aux);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
struct Supplemental_Page_Table_Entry *setup_spte(void *kpage);
bool install_page(void *upage, void *kpage, bool writable);
bool handle_mm_fault(struct Supplemental_Page_Table_Entry *spte);

#endif