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
    MEMORY, /* ALL READY IN MEMORY, HAS A FRAME */
    SWAP,   /* IN THE SWAP SPACE*/
    DISK    /* FROM A FILE */
    // the status in page.c will tell the frame (in a switch case) what direction to go: SWAP, MEMORY, DISK
};
enum source
{
    ELF,     /* File Backed */
    GENERAL, /* STACK or Variables?? */
    // ? Not sure if Anonymous is needed
    // ANONYMOUS /* IN the swap SPACE */

};

struct Supplemental_Page_Table_Entry
{
    struct hash_elem hash_elem;
    uint32_t key; /* Key */

    void *uaddr;                 /* User Address - Virtual Page */
    void *kaddr;                 /* Kernel Address - Frame ?? */
    enum page_location location; /* Where it currently is */
    enum source source;          /* Where it came from */
    bool dirty;                  /* Was it written to */

    struct lock lock; // Might need

    // TODO: Could add it Frame_Table_Entry once implemented

    /*
    & APEMAN MONEUVERS
    */
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    bool writable;
};

bool load_page(void *kaddr, struct Supplemental_Page_Table_Entry *spte);
bool page_hash(const struct hash_elem *p_, void *aux);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
static bool setup_stack_page(void **esp, int argc, char *argv[]);
struct Supplemental_Page_Table_Entry *setup_spte_general(void *addr);
bool install_page(void *upage, void *kpage, bool writable);
bool handle_mm_fault(void *addr);
struct Supplemental_Page_Table_Entry *find_spte(struct hash hash, void *addr);
struct Supplemental_Page_Table_Entry *setup_spte_general(void *addr);
struct Supplemental_Page_Table_Entry *setup_spte_from_file(void *upage, struct file *file, off_t ofs, bool writable, uint32_t read_bytes);

#endif