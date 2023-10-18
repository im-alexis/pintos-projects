#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys/directory.h"

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
bool load_file(void* kaddr, struct Supplementary_Page_Table *spte){

     
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
