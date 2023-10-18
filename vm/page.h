#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
// from Ed Discussion we declared the SPTE struct 



enum page_location{
    something,
    somethings
     // delete when we find out what really goes here
    // ! is this 
    // SWAP
    // MEMORY
    // DISK
    // the status in page.c will tell the frame (in a switch case) what direction to go: SWAP, MEMORY, DISK
};

struct Supplementary_Page_Table{
struct hash_elm *hash_elem;
/*

*Location of each page: whether it’s in SWAP, MEMORY, or DISK  // does this mmean enum page_location? aka done?
! Uaddr and Kaddr (Done?? line 17 & 18)
*Etc. (Don't know what this means)

*/
void *uaddr;
void *kaddr;

enum page_location status;

};

bool load_file(void* kaddr, struct Supplementary_Page_Table *spte);


#endif