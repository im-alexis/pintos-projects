#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include <bitmap.h>
#include "vm/page.h"

#include "threads/synch.h"
#include "threads/palloc.h"

struct frame_table_type
{
    struct list frames;            /* Left here per Alexis' request. Would be used if decided to use list implementation. */
    struct hash frame_hash;        /* Hashmap that holds the frame table */
    struct bitmap *occupied;       /* Bitmap that keeps track of occupied frames. */
    uint32_t clock_hand_key;       /* Hash element key for clock hand. */
    uint32_t how_many_pages_taken; // Why is this necessary?
};

struct frame_table_entry
{
    struct hash_elem hash_elem;
    uint32_t key; /* Key */ /* Can be used as the index of in the frame table */

    void *frame_addr;
    struct Supplemental_Page_Table_Entry *spte;
    struct list_elem frame;

    uint32_t clock_index;
};

/* Functions for Frame manipulation. */

bool frame_hash(const struct hash_elem *p_, void *aux);

bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux);

/* Initializes the virtual memory frame system. */
void vm_frame_init(size_t user_page_limit);

/* Initializes the frame table. */
struct frame_table_entry *vm_frame_table_entry_init(void *addr);

/* Allocates a frame for the given user page, using specified allocation flags. */
void *vm_frame_allocate(enum palloc_flags flags);

struct frame_table_entry *vm_find_eviction_frame(struct frame_table_type *ft);

/* Frees a previously allocated frame. */
void vm_frame_free(void *kpage);

/* Removes a frame entry from the frame table. */
void vm_frame_remove_entry(void *kpage);

/* Pins a frame in memory to prevent eviction. */
void vm_frame_pin(void *kpage);

/* Unpins a previously pinned frame, allowing it to be considered for eviction. */
void vm_frame_unpin(void *kpage);

struct frame_table_entry *vm_frame_eviction(struct frame_table_entry *frame);

#endif
/*
Eviction
targer = frame
1) Clear pagedir of target -> lock
2) update target SPT ->
    i) -> swap if dirty
    ii) ->disk  if not
3) copy new page in (load)
4) install page
5) update spte
*/