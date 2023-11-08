#include "frame.h"
#include "threads/init.h"
#include "vm/page.h"
#include "threads/thread.h"
#include <hash.h>
#define LOGGING_LEVEL 6

#include <log.h>

/*
Plan:
    Figure out what is the page number that needs to be passed in;
    if it's page number, how do we obtain that? I don't think it's uaddr.
    Do the swap algorithm.
    Synchronize.

*/

bool frame_hash(const struct hash_elem *p_, void *aux)
{
    const struct frame_table_entry *p = hash_entry(p_, struct frame_table_entry, hash_elem);
    return hash_bytes(&p->key, sizeof(p->key));
}

/*
 & Returns true if foo a precedes foo b.
 */
bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux)
{
    const struct frame_table_entry *a = hash_entry(a_, struct frame_table_entry, hash_elem);
    const struct frame_table_entry *b = hash_entry(b_, struct frame_table_entry, hash_elem);

    return a->key < b->key;
}

void vm_frame_init(size_t user_page_limit)
{
    palloc_init(user_page_limit);
}

struct frame_table_entry *vm_frame_table_entry_init(void *addr)
{
    // log(L_TRACE, "vm_frame_table_entry_init(addr: [%08x])", addr);
    struct frame_table_entry *fte = (struct frame_table_entry *)malloc(sizeof(struct frame_table_entry));
    fte->frame_addr = addr;
    fte->spte = NULL;
    return fte;
}

struct frame_table_entry *find_fte(struct hash hash, uint32_t key)
{
    log(L_TRACE, "find_fte(hash: [%08x], key: [%d])", hash, key);
    struct frame_table_entry scratch;
    struct frame_table_entry *result = NULL;

    struct hash_elem *e;
    scratch.key = key;
    // og(L_TRACE, "Key in Exeception.c:[%04x]\n", scratch.key);
    e = hash_find(&hash, &scratch.hash_elem);
    // log(L_TRACE, "e:[%d]\n", e);
    if (e != NULL)
    {
        result = hash_entry(e, struct frame_table_entry, hash_elem);
        log(L_DEBUG, "Frame ADDR: [%08x] , FTE was found", result->frame_addr);
    }
    return result;
}

void *vm_frame_allocate(enum palloc_flags flags)
{
    /*
    ! PICKLE IS HERE, the same addr is being sent back [c0271000]
    */
    log(L_TRACE, "vm_frame_allocate(flags: [%d])", flags);
    void *frame_addr = NULL;

    /* Returns true if there are any bits between idx 0 - 367 that are not occupied. */
    if (bitmap_contains(frameTable->occupied, 0, 367, false))
    {
        /* Obtains the first unoccupied bitmap index. */
        uint32_t key = bitmap_scan(frameTable->occupied, 0, 1, false);

        /* Flips the bit to 1 - marks occupied. */
        bitmap_flip(frameTable->occupied, key);
        struct frame_table_entry *fte = find_fte(frameTable->frame_hash, key);
        if (fte != NULL)
        {

            frame_addr = fte->frame_addr;
            // fte->spte = spte;
        }
    }
    else
    {
        struct frame_table_entry *victim = vm_find_eviction_frame(frameTable);
        if (victim != NULL)
        {
            frame_addr = vm_frame_eviction(victim);
            // victim->spte = spte;
        }
    }
    return frame_addr;
}

/*
    Returns the frame table of the frame chosen to be evicted (clock algorithm).
    First checks to see if there are any pages that are not dirty and not recently used.
    If there are none, it will choose the least recently used page.
    Note: It resets recently used in first search, not dirty pages, so it will choose clean pages first.
    ***Note: need to check for pinned frames.
    Written by Lupita, so if its terrible, pls be nice bc i might cry :,)
*/
struct frame_table_entry *vm_find_eviction_frame(struct frame_table_type *ft)
{
    log(L_TRACE, "vm_find_eviction_frame(ft: [%08x])", ft);
    struct frame_table_entry *victim = NULL;     /* Frame to be evicted. */
    struct frame_table_entry *curr_frame = NULL; /* Current frame being checked to see if good eviction candidate. */
    struct thread *t = thread_current();         /* Obtains the executing thread. */
    uint32_t size = 367;                         /* Number of physical user frames. */
    uint32_t index = 0;

    /* Try to find a frame that is not recently used and not dirty. */
    for (index = 0; index < size; index++)
    {
        uint32_t key = ((ft->clock_hand_key) + index) % 367;

        /* Find the frame in the hash table */
        curr_frame = find_fte(ft->frame_hash, key);
        void *suspect = curr_frame->spte->uaddr;

        if (suspect != NULL && !(pagedir_is_dirty(t->pagedir, suspect)))
        {
            if (pagedir_is_accessed(t->pagedir, suspect))
            {
                /* Reset the page to be noted as not recently used. */
                pagedir_set_accessed(t->pagedir, suspect, false);
            }
            else
            {
                /* Update frame table clock hand to be the most recently evicted frame. */
                ft->clock_hand_key = curr_frame->key;
                /* Suspect is not dirty and not recently used. Select to be evicted. */
                return curr_frame;
            }
        }
        else if (suspect == NULL)
        {
            return NULL; /* ERROR */
        }
        else
        {
            /* Page is dirty */
            pagedir_set_accessed(t->pagedir, suspect, true);
        }
    }

    /* Reset index to original clock hand since we didn't find a victim. */
    index = (ft->clock_hand_key);

    while (victim == NULL)
    {
        uint32_t key = index % 367;

        /* Obtain the next frame for potential eviction. */
        curr_frame = find_fte(ft->frame_hash, key);
        void *suspect = curr_frame->spte->uaddr;

        if (suspect != NULL && pagedir_is_accessed(t->pagedir, suspect))
        {
            /* Reset the accessed bit of the current page. */
            pagedir_set_accessed(t->pagedir, suspect, false);
        }
        else if (suspect != NULL)
        {
            /* Found eviction victim. */
            victim = curr_frame;
        }
        else
        {
            /* ERROR */
            return NULL;
        }

        index++;
    }

    ft->clock_hand_key = victim->key; /* Update frame table clock hand to be the most recently evicted frame. */

    return victim;
}

/*
    Function to complete frame eviction:
    1. Remove any references to the chose frame from any page tables.
    2. If necessary, move current table to the swap space/file system.
    3. Load new page into the newly freed frame.
*/
struct frame_table_entry *vm_frame_eviction(struct frame_table_entry *frame)
{
    log(L_TRACE, "vm_frame_eviction(frame: [%08x])", frame);

    struct thread *t = thread_current();
    struct Supplemental_Page_Table_Entry *victim = frame->spte;

    if (pagedir_is_dirty(t->pagedir, victim))
    {
        /* DO: SWAP ALGORITHM*/
    }
    else
    {
        victim->location = DISK;
        victim->kaddr = NULL;
    }
}