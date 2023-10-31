#include "frame.h"
#include "threads/init.h"
#include "vm/page.h"
#include <hash.h>
// I think here is where we do all the hashing stuff
// I've pre-built some of the .h stuff we may need

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
    struct frame_table_entry *fte = (struct frame_table_entry *)malloc(sizeof(struct frame_table_entry));
    fte->frame_addr = addr;
    fte->spte = NULL;
    return NULL;
}

struct frame_table_entry *find_fte(struct hash hash, uint32_t key)
{
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
        // log(L_DEBUG, "KPAGE: [%08x] | UPAGE: [%08x] | Key:[%08x], spte was found", result->kaddr, result->uaddr, result->key);
    }
    return result;
}

void *vm_frame_allocate(enum palloc_flags flags, struct Supplemental_Page_Table_Entry *spte)
{
    void *frame_addr = NULL;
    if (frameTable->how_many_pages_taken < 366)
    {
        uint32_t next_key = frameTable->how_many_pages_taken + 1;
        struct frame_table_entry *fte = find_fte(frameTable->frame_hash, next_key);
        if (fte != NULL)
        {
            frame_addr = fte->frame_addr;
            fte->spte = spte;
        }
    }
    else
    {
        // Clock_algy
    }
    return frame_addr;
}

struct frame_table_entry *vm_frame_eviction(struct frame_table_type ft)
{
}
