#include <debug.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lib/stdio.h"
#include "filesys/filesys.h"

#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
#define LOGGING_LEVEL 6
#include <log.h>

/* Random value for struct thread's `magic' member.
 * Used to detect stack overflow.  See the big comment at the top
 * of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
 * that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
 * when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
{
    void *eip;             /* Return address. */
    thread_func *function; /* Function to call. */
    void *aux;             /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4          /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
 * If true, use multi-level feedback queue scheduler.
 * Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);

static struct thread *running_thread(void);

static struct thread *next_thread_to_run(void);

static void init_thread(struct thread *, const char *name, int priority);

// static bool is_thread(struct thread *) UNUSED;

static void *alloc_frame(struct thread *, size_t size);

static void schedule(void);

void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);

/* Initializes the threading system by transforming the code
 * that's currently running into a thread.  This can't work in
 * general and it is possible in this case only because loader.S
 * was careful to put the bottom of the stack at a page boundary.
 *
 * Also initializes the run queue and the tid lock.
 *
 * After calling this function, be sure to initialize the page
 * allocator before trying to create any threads with
 * thread_create().
 *
 * It is not safe to call thread_current() until this function
 * finishes. */
void thread_init(void) // 4. we can do the suma init here
{
    ASSERT(intr_get_level() == INTR_OFF);

    lock_init(&tid_lock);
    list_init(&ready_list);
    list_init(&all_list);

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread();
    init_thread(initial_thread, "main", PRI_DEFAULT);
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
 * Also creates the idle thread. */
void thread_start(void)
{
    log(L_TRACE, "thread_start");
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init(&idle_started, 0);
    thread_create("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
 * Thus, this function runs in an external interrupt context. */
void thread_tick(void)
{
    struct thread *t = thread_current();

    /* Update statistics. */
    if (t == idle_thread)
    {
        idle_ticks++;
    }
#ifdef USERPROG
    else if (t->pagedir != NULL)
    {
        user_ticks++;
    }
#endif
    else
    {
        kernel_ticks++;
    }

    /* Enforce preemption. */
    if (++thread_ticks >= TIME_SLICE)
    {
        intr_yield_on_return();
    }
}

/* Prints thread statistics. */
void thread_print_stats(void)
{
    printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
           idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
 * PRIORITY, which executes FUNCTION passing AUX as the argument,
 * and adds it to the ready queue.  Returns the thread identifier
 * for the new thread, or TID_ERROR if creation fails.
 *
 * If thread_start() has been called, then the new thread may be
 * scheduled before thread_create() returns.  It could even exit
 * before thread_create() returns.  Contrariwise, the original
 * thread may run for any amount of time before the new thread is
 * scheduled.  Use a semaphore or some other form of
 * synchronization if you need to ensure ordering.
 *
 * The code provided sets the new thread's `priority' member to
 * PRIORITY, but no actual priority scheduling is implemented.
 * Priority scheduling is the goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority,
                    thread_func *function, void *aux)
{
    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;

    ASSERT(function != NULL);

    /* Allocate thread. */
    t = palloc_get_page(PAL_ZERO);
    if (t == NULL)
    {
        return TID_ERROR;
    }

    /* Initialize thread. */
    init_thread(t, name, priority);
    tid = t->tid = allocate_tid();

    /* Stack frame for kernel_thread(). */
    kf = alloc_frame(t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame(t, sizeof *ef);
    ef->eip = (void (*)(void))kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame(t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    if (thread_current()->current_dir)
    {
        log(L_DEBUG, "in thread_create(), thread getting parent directory");
        t->current_dir = dir_reopen(thread_current()->current_dir);
    }
    else
    {
        log(L_DEBUG, "in thread_create(), thread getting NULL Directory");
        t->current_dir = NULL;
    }

    /* Add to run queue. */
    thread_unblock(t);

    /*
    Maybe set some initialization here

    */

    return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
 * again until awoken by thread_unblock().
 *
 * This function must be called with interrupts turned off.  It
 * is usually a better idea to use one of the synchronization
 * primitives in synch.h. */
void thread_block(void)
{
    ASSERT(!intr_context());
    ASSERT(intr_get_level() == INTR_OFF);

    thread_current()->status = THREAD_BLOCKED;
    schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
 * This is an error if T is not blocked.  (Use thread_yield() to
 * make the running thread ready.)
 *
 * This function does not preempt the running thread.  This can
 * be important: if the caller had disabled interrupts itself,
 * it may expect that it can atomically unblock a thread and
 * update other data. */
void thread_unblock(struct thread *t)
{
    enum intr_level old_level;

    ASSERT(is_thread(t));

    old_level = intr_disable();
    ASSERT(t->status == THREAD_BLOCKED);
    list_push_back(&ready_list, &t->elem);
    t->status = THREAD_READY;
    intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name(void)
{
    return thread_current()->name;
}

/* Returns the running thread.
 * This is running_thread() plus a couple of sanity checks.
 * See the big comment at the top of thread.h for details. */
struct thread *
thread_current(void)
{
    struct thread *t = running_thread();

    /* Make sure T is really a thread.
     * If either of these assertions fire, then your thread may
     * have overflowed its stack.  Each thread has less than 4 kB
     * of stack, so a few big automatic arrays or moderate
     * recursion can cause stack overflow. */
    ASSERT(is_thread(t));
    ASSERT(t->status == THREAD_RUNNING);

    return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void)
{
    return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
 * returns to the caller. */
void thread_exit(void)
{
    ASSERT(!intr_context());

#ifdef USERPROG

    /*
    Might need to dealloacate some stuff
    */
    process_exit();
#endif

    /* Remove thread from all threads list, set our status to dying,
     * and schedule another process.  That process will destroy us
     * when it calls thread_schedule_tail(). */
    intr_disable();
    list_remove(&thread_current()->allelem);
    thread_current()->status = THREAD_DYING;
    schedule();
    NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
 * may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void)
{
    struct thread *cur = thread_current();
    enum intr_level old_level;

    ASSERT(!intr_context());

    old_level = intr_disable();
    if (cur != idle_thread)
    {
        list_push_back(&ready_list, &cur->elem);
    }
    cur->status = THREAD_READY;
    schedule();
    intr_set_level(old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
 * This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux)
{
    struct list_elem *e;

    ASSERT(intr_get_level() == INTR_OFF);

    for (e = list_begin(&all_list); e != list_end(&all_list);
         e = list_next(e))
    {
        struct thread *t = list_entry(e, struct thread, allelem);
        func(t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority)
{
    thread_current()->priority = new_priority;
}

/* Returns the current thread's priority. */
int thread_get_priority(void)
{
    return thread_current()->priority;
}

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice UNUSED)
{
    /* Not yet implemented. */
}

/* Returns the current thread's nice value. */
int thread_get_nice(void)
{
    /* Not yet implemented. */
    return 0;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void)
{
    /* Not yet implemented. */
    return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void)
{
    /* Not yet implemented. */
    return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.
 *
 * The idle thread is initially put on the ready list by
 * thread_start().  It will be scheduled once initially, at which
 * point it initializes idle_thread, "up"s the semaphore passed
 * to it to enable thread_start() to continue, and immediately
 * blocks.  After that, the idle thread never appears in the
 * ready list.  It is returned by next_thread_to_run() as a
 * special case when the ready list is empty. */
static void
idle(void *idle_started_ UNUSED)
{
    struct semaphore *idle_started = idle_started_;

    idle_thread = thread_current();
    sema_up(idle_started);

    for (;;)
    {
        /* Let someone else run. */
        intr_disable();
        thread_block();

        /* Re-enable interrupts and wait for the next one.
         *
         * The `sti' instruction disables interrupts until the
         * completion of the next instruction, so these two
         * instructions are executed atomically.  This atomicity is
         * important; otherwise, an interrupt could be handled
         * between re-enabling interrupts and waiting for the next
         * one to occur, wasting as much as one clock tick worth of
         * time.
         *
         * See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         * 7.11.1 "HLT Instruction". */
        asm volatile("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread(thread_func *function, void *aux)
{
    ASSERT(function != NULL);

    intr_enable(); /* The scheduler runs with interrupts off. */
    function(aux); /* Execute the thread function. */
    thread_exit(); /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread(void)
{
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
     * down to the start of a page.  Because `struct thread' is
     * always at the beginning of a page and the stack pointer is
     * somewhere in the middle, this locates the curent thread. */
    asm("mov %%esp, %0" : "=g"(esp));
    return pg_round_down(esp);
}

/* Returns true if T appears to point to a valid thread. */
bool is_thread(struct thread *t)
{
    return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
 * NAME. */
static void
init_thread(struct thread *t, const char *name, int priority)
{
    enum intr_level old_level;

    ASSERT(t != NULL);
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT(name != NULL);

    memset(t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy(t->name, name, sizeof t->name);
    t->executing_file = name;
    t->stack = (uint8_t *)t + PGSIZE;
    t->priority = priority;
    t->magic = THREAD_MAGIC;

    /*
   ? Additions made to code, when intialization of thread
    */

    t->has_been_waited_on = false;
    for (int i = 2; i < 20; i++)
    {
        // t->file_descriptor_table[i] = NULL;
        t->file_descriptor_table_plus[i] = NULL;
    }

    // t->fdt_index = 2;
    // t->how_many_fd = 2;

    t->fdt_index_plus = 2;
    t->how_many_fd_plus = 2;
    t->exit_code = -1;
    t->process_executing_file = NULL;
    // ADDED
    // inherit parent working dir

    // t->parent_executing_file = NULL;

    /*semephore initialiazation*/
    sema_init(&t->reading_exit_status, 0);
    sema_init(&t->exiting_thread, 0);
    sema_init(&t->process_semma, 0);

    /* Initialize the child thread list */
    list_init(&t->mis_ninos);

    /*Initialize the list of all threads f*/
    list_init(&t->all_process_list);
    list_push_back(&t->all_process_list, &t->allelem);

    /*
    ? Additions made to code, when intialization of thread
     */

    old_level = intr_disable();
    list_push_back(&all_list, &t->allelem);
    intr_set_level(old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
 * returns a pointer to the frame's base. */
static void *
alloc_frame(struct thread *t, size_t size)
{
    /* Stack data is always allocated in word-size units. */
    ASSERT(is_thread(t));
    ASSERT(size % sizeof(uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
 * return a thread from the run queue, unless the run queue is
 * empty.  (If the running thread can continue running, then it
 * will be in the run queue.)  If the run queue is empty, return
 * idle_thread. */
static struct thread *
next_thread_to_run(void)
{
    if (list_empty(&ready_list))
    {
        return idle_thread;
    }
    else
    {
        return list_entry(list_pop_front(&ready_list), struct thread, elem);
    }
}

/* Completes a thread switch by activating the new thread's page
 * tables, and, if the previous thread is dying, destroying it.
 *
 * At this function's invocation, we just switched from thread
 * PREV, the new thread is already running, and interrupts are
 * still disabled.  This function is normally invoked by
 * thread_schedule() as its final action before returning, but
 * the first time a thread is scheduled it is called by
 * switch_entry() (see switch.S).
 *
 * It's not safe to call printf() until the thread switch is
 * complete.  In practice that means that printf()s should be
 * added at the end of the function.
 *
 * After this function and its caller returns, the thread switch
 * is complete. */
void thread_schedule_tail(struct thread *prev)
{
    struct thread *cur = running_thread();

    ASSERT(intr_get_level() == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
    process_activate();
#endif

    /* If the thread we switched from is dying, destroy its struct
     * thread.  This must happen late so that thread_exit() doesn't
     * pull out the rug under itself.  (We don't free
     * initial_thread because its memory was not obtained via
     * palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
        ASSERT(prev != cur);
        palloc_free_page(prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
 * the running process's state must have been changed from
 * running to some other state.  This function finds another
 * thread to run and switches to it.
 *
 * It's not safe to call printf() until thread_schedule_tail()
 * has completed. */
static void
schedule(void)
{
    struct thread *cur = running_thread();
    struct thread *next = next_thread_to_run();
    struct thread *prev = NULL;

    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(cur->status != THREAD_RUNNING);
    ASSERT(is_thread(next));

    if (cur != next)
    {
        prev = switch_threads(cur, next);
    }
    thread_schedule_tail(prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid(void)
{
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire(&tid_lock);
    tid = next_tid++;
    lock_release(&tid_lock);

    return tid;
}
/*
Search for a thread given a tid
Returns a struct thread*
*/
struct thread *find_thread_by_tid(tid_t ftid)
{
    struct thread *cur = thread_current();
    struct thread *thread;

    /* Makes use of the list struct in kernel */
    struct list_elem *thread_in_list = list_begin(&cur->all_process_list); // set the entry for the list
    thread = list_entry(thread_in_list, struct thread, allelem);

    while (thread != NULL)
    {
        thread = list_entry(thread_in_list, struct thread, allelem);
        if (!is_thread(thread))
        {
            break;
        }

        if (thread->tid == ftid)
        {
            return thread;
        }

        thread_in_list = list_next(&thread->allelem);
    }
    return NULL;
}

/* Offset of `stack' member within `struct thread'.
 * Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);
int find_next_table_pos();
int find_next_table_pos()
{
    struct thread *cur = thread_current();
    for (int i = 2; i < MAX_FD; i++)
    {
        if (cur->file_descriptor_table[i] == NULL)
        {
            cur->fdt_index = i;
            return i;
        }
    }
    return -1;
}

int add_to_table(struct file *new_file)
{
    struct thread *cur = thread_current();
    // add like a loop around
    if ((cur->fdt_index == MAX_FD - 1) && (cur->how_many_fd != MAX_FD))
    {
        find_next_table_pos();
    }
    else if (new_file != NULL && (cur->how_many_fd < MAX_FD))
    {
        int val = cur->fdt_index;

        cur->file_descriptor_table[val] = new_file;
        cur->fdt_index++;
        cur->how_many_fd++;
        return val;
    }
    return -1;
}
void removed_from_table(int fd)
{
    struct thread *cur = thread_current();
    cur->file_descriptor_table[fd] = NULL;
    cur->how_many_fd--;
}
bool removed_from_table_by_file(struct file *file)
{
    struct thread *cur = thread_current();
    for (int i = 2; i < MAX_FD; i++)
    {
        struct file *looped_file = cur->file_descriptor_table[i];
        if (looped_file->inode == file->inode)
        {
            /*
            Properly rmeove from table
            */
            // free(cur->file_descriptor_table[i]);
            cur->file_descriptor_table[i] = NULL;
            cur->how_many_fd--;
            return true;
        }
    }

    return false;
}

int search_by_file(struct file *target_file)
{
    struct thread *cur = thread_current();
    for (int i = 2; i < MAX_FD; i++)
    {
        if (cur->file_descriptor_table[i] == target_file)
        {
            return i;
        }
    }
    return -1;
}

/*
& NEW FUNCTIONs w/ FILE_PLUS types
*/

/*
Given a file searches the fdt+ for a matching file given a name
returns index in fdt+ if found
else return -1
*/
int search_by_filename_plus(char *target_file)
{
    log(L_TRACE, "search_by_filename_plus(target_file: [%s])", target_file);
    struct thread *cur = thread_current();
    for (int i = 2; i < cur->how_many_fd_plus; i++)
    {
        if (!strcmp(cur->file_descriptor_table_plus[i]->name, target_file))
        {
            log(L_INFO, "Target_File: [%s], found in FDT+", target_file);
            return i;
        }
    }
    return -1;
}

/*
Given a file name searches the fdt+ for a matching file given a name
returns index in fdt+ if found
else return -1
*/
bool removed_from_table_by_filename_plus(char *filename)
{
    log(L_TRACE, "removed_from_table_by_filename_plus(filename: [%s])", filename);
    struct thread *cur = thread_current();
    int fd = search_by_filename_plus(filename);
    if (fd != -1)
    {
        cur->file_descriptor_table_plus[fd] = NULL;
        cur->how_many_fd_plus--;
        return true;
    }

    return false;
}
/*
Initializes a new file_plus struct, given a filename and struct file
*/
struct file_plus *create_file_plus(struct file *file, char *filename)
{
    log(L_TRACE, "create_file_plus(file: [%08x], filename: [%s])", file, filename);
    struct file_plus *new_file = malloc(sizeof(struct file_plus));
    new_file->file = file;

    char *name = malloc(strlen(filename) + 1);
    strlcpy(name, filename, strlen(filename) + 1);
    new_file->name = name;
    // memcpy(&new_file->name, filename, sizeof(filename));
    log(L_DEBUG, "file_plus:(file: [%08x], name: [%s])", new_file->file, new_file->name);
    return new_file;
}
/*
Given a file descriptor, it removes
*/
void removed_from_table_plus(int fd)
{
    log(L_TRACE, "removed_from_table_plus(fd: [%d])", fd);
    struct thread *cur = thread_current();
    cur->file_descriptor_table_plus[fd] = NULL;
    cur->how_many_fd_plus--;
}

int find_next_table_pos_plus();
int find_next_table_pos_plus()
{
    log(L_TRACE, "find_next_table_pos_plus()");
    struct thread *cur = thread_current();
    int ret = -1;
    for (int i = 2; i < MAX_FD; i++)
    {
        if (cur->file_descriptor_table_plus[i] == NULL)
        {
            cur->fdt_index_plus = i;
            ret = i;
            break;
        }
    }
    log(L_DEBUG, "ret: %d", ret);
    return ret;
}

int add_to_table_plus(struct file_plus *new_file)
{
    log(L_TRACE, "add_to_table_plus(file_plus: [file:[%08x], name:[%s]])", new_file->file, new_file->name);
    struct thread *cur = thread_current();
    int val = -1;
    // add like a loop around
    if ((cur->fdt_index_plus == MAX_FD - 1) && (cur->how_many_fd_plus != MAX_FD))
    {
        log(L_DEBUG, "FDT+ needs a index reset");
        find_next_table_pos_plus();
    }
    else if (new_file != NULL && (cur->how_many_fd_plus < MAX_FD))
    {
        log(L_DEBUG, "Assing a FDT+ index ");
        val = cur->fdt_index_plus;

        cur->file_descriptor_table_plus[val] = new_file;
        cur->fdt_index_plus++;
        cur->how_many_fd_plus++;
        // return val;
    }
    log(L_DEBUG, "File descriptor Index Assinged: %d", val);
    return val;
}

void destroy_plus_file(struct file_plus *pfile, bool close_file)
{
    log(L_TRACE, "destroy_plus_file(file_plus: [file:[%08x], name[%s]], bool: [%d])", pfile->file, pfile->name, close_file);
    if (close_file)
    {
        file_close(pfile->file);
    }
    free(pfile->name);
    free(pfile);
}

/*
Closes all files in a FDT+
 */
void close_thread_files()
{
    struct thread *cur = thread_current;
    if (cur->how_many_fd == 2)
    {
        return;
    }
    for (int i = 2; i < MAX_FD; i++)
    {
        struct file_plus *file = cur->file_descriptor_table_plus[i];
        if (file != NULL)
        {
            destroy_plus_file(file, true);
            cur->file_descriptor_table[i] = NULL;
            cur->how_many_fd--;
        }
        if (cur->how_many_fd == 2)
            break;
    }
}