#pragma once
#include "../stdint.h"

#define PRIORITY_IDLE       0
#define PRIORITY_LOW        8
#define PRIORITY_NORMAL     16
#define PRIORITY_HIGH       24
#define PRIORITY_REALTIME   31
#define NUM_PRIORITY_LEVELS 32

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_EXITED
} task_state_t;

typedef struct task {
    uint32_t tid;
    uint32_t priority;
    uint32_t base_priority;
    task_state_t state;
    
    uint64_t rip;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rflags;
    
    uint64_t stack_base;
    uint64_t stack_top;
    uint32_t stack_size;
    
    uint32_t time_slice;
    uint32_t time_slice_max;
    uint32_t total_ticks;
    uint32_t context_switches;
    
    uint64_t wakeup_time;
    
    struct task *next;
    struct task *prev;
    
    uint64_t cr3;
    
    uint32_t creation_ticks;
    uint32_t start_ticks;
} task_t;

typedef struct {
    task_t *head;
    task_t *tail;
    uint32_t count;
} ready_queue_t;

typedef struct {
    ready_queue_t ready_queues[NUM_PRIORITY_LEVELS];
    
    task_t *current_task;
    
    uint32_t next_tid;
    
    uint64_t system_ticks;
    uint32_t preemptions;
    uint32_t context_switches;
    
    uint32_t highest_priority_ready;
    uint32_t total_ready_tasks;
} scheduler_t;

#define SCHED_QUANTUM_TICKS 2

void sched_init(void);
task_t* sched_create_task(uint64_t entry_point, uint32_t priority, uint32_t stack_size);
int sched_destroy_task(uint32_t tid);
void sched_tick(void);
void sched_yield(void);
void sched_block_until(uint64_t wakeup_ticks);
void sched_unblock_task(task_t *task);
int sched_set_priority(uint32_t tid, uint32_t new_priority);

static inline task_t* sched_current_task(void) {
    extern scheduler_t kernel_scheduler;
    return kernel_scheduler.current_task;
}

static inline uint64_t sched_ticks(void) {
    extern scheduler_t kernel_scheduler;
    return kernel_scheduler.system_ticks;
}

extern scheduler_t kernel_scheduler;

extern void sched_context_switch(task_t *prev, task_t *next);
extern void sched_restore_context(task_t *task);
extern uint64_t sched_get_stack_pointer(void);