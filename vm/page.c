#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/tss.h"
#include "lib/kernel/list.h"
#define LOGGING_LEVEL 6

#include <log.h>

static bool setup_stack(void **esp, int argc, char *argv[])
{

    uint8_t *kpage;
    bool success = false;

    log(L_TRACE, "setup_stack()");

    kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage != NULL)
    {
        success = install_page(((uint8_t *)PHYS_BASE) - PGSIZE, kpage, true);
        if (success)

        {
            /* Offset PHYS_BASE as instructed. */
            //*esp = PHYS_BASE - 12;
            *esp = PHYS_BASE;

            /* A list of addresses to the values that are initially added to the stack.  */
            uint32_t *arg_val_ptr[argc];
            uint32_t byte_count = 0;

            /* First add all of the command line arguments in descending order, including
               the program name. */
            for (int i = argc - 1; i >= 0; i--)
            {
                /* Allocate enough space for the entire string (plus an extra byte for
                   '/0'). Copy the string to the stack, and add its reference to the array
                    of pointers. Keep track of how many bytes where needed for the arguments */

                byte_count = byte_count + sizeof(char) * (strlen(argv[i]) + 1);

                *esp = *esp - sizeof(char) * (strlen(argv[i]) + 1);
                memcpy(*esp, argv[i], sizeof(char) * (strlen(argv[i]) + 1));
                arg_val_ptr[i] = (uint32_t *)*esp;
            }

            // This padding for 4 byte allingment
            uint32_t padding_need = byte_count % 4;
            for (int i = 0; i < padding_need; i++)
            {
                *esp = *esp - sizeof(char);
                (*(char *)(*esp)) = 0;
            }

            // Allocate space for & add the null sentinel.
            *esp = *esp - 4;
            (*(int *)(*esp)) = 0;

            /* Push onto the stack each char* in arg_value_pointers[] (each of which
               references an argument that was previously added to the stack). */
            *esp = *esp - 4;
            for (int i = argc - 1; i >= 0; i--)
            {
                (*(uint32_t **)(*esp)) = arg_val_ptr[i];
                *esp = *esp - 4;
            }

            /* Push onto the stack a pointer to the pointer of the address of the
               first argument in the list of arguments. */
            (*(uintptr_t **)(*esp)) = *esp + 4;

            *esp = *esp - 4;
            *(int *)(*esp) = argc; // put how many arguments there are

            // Push onto the stack a fake return address
            *esp = *esp - 4;
            (*(int *)(*esp)) = 0; // put the return address at the end

            /*USER STACK SHOULD BE SETUP BY NOW*/
        }

        else
        {
            palloc_free_page(kpage);
        }
        // hex_dump(*(int *)esp, *esp, 128, true); // NOTE: uncomment this to check arg passing
    }

    /* Create vm entry*/
    /* Set up vm_entry*/
    /* Using insert_vme(), add vm_entry to hash table*/
    return success;
}
/*
! Add Suplemental Page Table to parameters
*/
bool load_file(void *kaddr)
{
}