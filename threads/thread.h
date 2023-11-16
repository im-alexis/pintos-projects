#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include "filesys/file.h"

// #include "filesys/experiment/file_plus.h"

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

struct file_plus
{
    struct file *file;
    char *name;
    bool writable;
};

struct file_plus *create_file_plus(struct file *file, char *filename);
bool removed_from_table_by_filename_plus(char *filename);
int search_by_filename_plus(char *target_file);
void removed_from_table_plus(int fd);
int add_to_table_plus(struct file_plus *new_file);
void destroy_plus_file(struct file_plus *pfile, bool close_file);
void close_thread_files();

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
    char *executing_file;         /* Holds the name of the executing file, might switch to actual file, but IDK */
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;          /* List element. */
    struct thread *parent;          /* The parant of this thread. */
    struct semaphore process_semma; /* Semaphore when calling start_process/process_execute combo. */

    void *stack_pointer;

    struct file *process_executing_file;
    // struct file *parent_executing_file;

    // struct Supplemental_Page_Table_Entry *entry; -> Don't think we need

#ifdef USERPROG
    /* Owned by userprog/process.c. */

    uint32_t *pagedir;                    /* Page directory. */
    struct semaphore exiting_thread;      /* For when the thread is exiting*/
    struct semaphore reading_exit_status; /* To make sure the parent can read the exit status of the child*/
    int exit_code;                        /* Holds the exit status for the thread*/

    struct list mis_ninos; /* List of child process belonging to this process */
    struct list_elem chld_thrd_elm;
    bool has_been_waited_on; /* Simple flag to check if a child was waited on or not*/

    struct file *file_descriptor_table[MAX_FD]; /* Holds File Descriptors per process*/
    int fdt_index;                              /* Is the index to the next file descriptor */
    int how_many_fd;                            /* Runnning count of how many files this process has open*/

    struct file_plus *file_descriptor_table_plus[MAX_FD];
    int fdt_index_plus;   /* Is the index to the next file descriptor */
    int how_many_fd_plus; /* Runnning count of how many files this process has open*/

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

int add_to_table(struct file *new_file);
void removed_from_table(int fd);
bool removed_from_table_by_file(struct file *file);
int search_by_file(struct file *target_file);

#endif /* threads/thread.h */