#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include "filesys/file.h"
#include "lib/kernel/hash.h"
#include "vm/page.h"

/* States in a thread's life cycle. */
enum thread_status
{
    THREAD_RUNNING, /* Running thread. */
    THREAD_READY,   /* Not running but ready to run. */
    THREAD_BLOCKED, /* Waiting for an event to trigger. */
    THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
 * You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

#define MAX_FD 128 /*Maximum number of file descriptors allowed per table*/
/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
    /* Owned by thread.c. */
    tid_t tid;                    /* Thread identifier. */
    enum thread_status status;    /* Thread state. */
    char name[16];                /* Name (for debugging purposes). */
    uint8_t *stack;               /* Saved stack pointer. */
    int priority;                 /* Priority. */
    struct list_elem allelem;     /* List element for all threads list. */
    struct list all_process_list; /* So that the thread can access all other threads*/
    char *executing_file;
    /* Shared between thread.c and synch.c. */
    struct list_elem elem; /* List element. */
    struct thread *parent;
    struct semaphore process_semma;
    struct hash spt_hash; // Hash Table to manage virtual address space of thread
    struct Supplementary_Page_Table *entry;
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir; /* Page directory. */

    /*
    Need to inititalize
     1. file_descriptor_table -> make of size 20
     2. fdt_index -> make default 3 (STDIN (0), STDOUT (1), STDERR (2) ....)
     3. Semaphores?
     4. has_been_waited_one
    */
    struct semaphore exiting_thread;      /* For when the thread is exiting*/
    struct semaphore reading_exit_status; /* To make sure the parent can read the exit status of the child*/
    int exit_code; /* Holds the exit status for the thread*/
    struct file *file_descriptor_table[MAX_FD]; /* Holds File Descriptors per process*/
    int fdt_index;
    int how_many_fd; /* Is the index to the next file descriptor */
    struct list mis_ninos;
    struct list_elem chld_thrd_elm;
    bool has_been_waited_on; /* Simple flag to check if a child was waited on or not*/

#endif

    /* Owned by thread.c. */
    unsigned magic; /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
 * If true, use multi-level feedback queue scheduler.
 * Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/* Could be useful to see all threads */
// extern ;

void thread_init(void);
void thread_start(void);
void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);
struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);
void thread_exit(void) NO_RETURN;
void thread_yield(void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);
void thread_foreach(thread_action_func *, void *);

int thread_get_priority(void);
void thread_set_priority(int);
int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

bool is_thread(struct thread *t);
struct thread *find_thread_by_tid(tid_t tid);

int add_to_table(struct thread *cur, struct file *new_file);
void removed_from_table(int fd, struct thread *cur);
bool removed_from_table_by_filename(struct thread *cur, struct file *file);
int search_by_file(struct thread *cur, struct file *target_file);

#endif /* threads/thread.h */
