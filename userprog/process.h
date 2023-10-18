#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "vm/frame.h"
#include "vm/page.h"

#include "filesys/directory.h"
#include "lib/kernel/hash.h"


tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

#endif /* userprog/process.h */
