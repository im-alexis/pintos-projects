#ifndef THREADS_INIT_H
#define THREADS_INIT_H

#include <debug.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vm/frame.h"
#include <bitmap.h>
#include <hash.h>


/* Page directory with kernel mappings only. */
extern uint32_t *init_page_dir;

extern struct frame_table_type *frameTable;

#endif /* threads/init.h */
