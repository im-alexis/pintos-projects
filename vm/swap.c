// swaping and inviction stuff? I added some stuff from Ed-Discussion, but a great place to
// start to look at the variables needed
#include "threads/vaddr.h"
#include "devices/block.h" // 4.1.1
#include "devices/block.c" // 4.1.1

// BLOCK_SECTOR_SIZE is in block.h
// PGSIZE / BLOCK_SECTOR_SIZE (=4KiB/512B = 8) 
// PGSIZE is a variable in vaddr?
static const size_t Sections_Per_Tabe = PGSIZE / BLOCK_SECTOR_SIZE; // (=4KiB/512B = 8) 
