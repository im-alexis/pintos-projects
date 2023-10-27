#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/palloc.h"

struct Fram_Table_Entry
{
    struct hash_elem hash_elem;
    uint32_t key; /* Key */
};

/* Functions for Frame manipulation. */

/* Initializes the virtual memory frame system. */
void vm_frame_init(void);

/* Allocates a frame for the given user page, using specified allocation flags. */
void *vm_frame_allocate(enum palloc_flags flags, void *upage);

/* Frees a previously allocated frame. */
void vm_frame_free(void *kpage);

/* Removes a frame entry from the frame table. */
void vm_frame_remove_entry(void *kpage);

/* Pins a frame in memory to prevent eviction. */
void vm_frame_pin(void *kpage);

/* Unpins a previously pinned frame, allowing it to be considered for eviction. */
void vm_frame_unpin(void *kpage);

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