#include "scheduler.h"
#include "../mm/alloc.h"

scheduler_t kernel_scheduler;

static void sched_memset(void *ptr, int val, uint32_t size) {
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < size; i++) p[i] = val;
}
static uint64_t sched_alloc_stack(uint32_t size) {
    return (uint64_t)mm_alloc(size);
}
static void sched_free_stack(uint64_t stack) {
    mm_free((void *)stack);
}
static void sched_enqueue(task_t *task) {
    if (!task) return;
    ready_queue_t *queue = &kernel_scheduler.ready_queues[task->priority];
    task->next = NULL;
    task->prev = queue->tail;
    if (queue->tail) {
        queue->tail->next = task;
    } else {
        queue->head = task;
    }
    queue->tail = task;
    queue->count++;
    kernel_scheduler.total_ready_tasks++;
    if (task->priority > kernel_scheduler.highest_priority_ready) {
        kernel_scheduler.highest_priority_ready = task->priority;
    }
}
static void sched_dequeue(task_t *task) {
    if (!task) return;
    ready_queue_t *queue = &kernel_scheduler.ready_queues[task->priority];
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        queue->head = task->next;
    }
    if (task->next) {
        task->next->prev = task->prev;
    } else {
        queue->tail = task->prev;
    }
    queue->count--;
    kernel_scheduler.total_ready_tasks--;
    task->next = NULL;
    task->prev = NULL;
}
static task_t* sched_pick_next(void) {
    for (int prio = NUM_PRIORITY_LEVELS - 1; prio >= 0; prio--) {
        ready_queue_t *queue = &kernel_scheduler.ready_queues[prio];
        if (queue->head) {
            return queue->head;
        }
    }
    return NULL;
}
static void sched_update_highest_priority(void) {
    kernel_scheduler.highest_priority_ready = 0;
    for (int prio = NUM_PRIORITY_LEVELS - 1; prio >= 0; prio--) {
        if (kernel_scheduler.ready_queues[prio].count > 0) {
            kernel_scheduler.highest_priority_ready = prio;
            break;
        }
    }
}
void sched_init(void) {
    sched_memset(&kernel_scheduler, 0, sizeof(scheduler_t));
    kernel_scheduler.next_tid = 1;
    kernel_scheduler.system_ticks = 0;
    kernel_scheduler.current_task = NULL;
    for (int i = 0; i < NUM_PRIORITY_LEVELS; i++) {
        kernel_scheduler.ready_queues[i].head = NULL;
        kernel_scheduler.ready_queues[i].tail = NULL;
        kernel_scheduler.ready_queues[i].count = 0;
    }
}
task_t* sched_create_task(uint64_t entry_point, uint32_t priority, uint32_t stack_size) {
    if (priority >= NUM_PRIORITY_LEVELS) {
        return NULL;
    }
    task_t *task = (task_t *)mm_alloc(sizeof(task_t));
    if (!task) {
        return NULL;
    }
    uint64_t stack = sched_alloc_stack(stack_size);
    if (!stack) {
        mm_free(task);
        return NULL;
    }
    sched_memset(task, 0, sizeof(task_t));
    task->tid = kernel_scheduler.next_tid++;
    task->priority = priority;
    task->base_priority = priority;
    task->state = TASK_READY;
    task->time_slice_max = SCHED_QUANTUM_TICKS;
    task->time_slice = SCHED_QUANTUM_TICKS;
    task->creation_ticks = kernel_scheduler.system_ticks;
    task->start_ticks = 0;
    task->stack_base = stack;
    task->stack_top = stack + stack_size;
    task->stack_size = stack_size;
    task->rsp = task->stack_top - 8;
    task->rip = entry_point;
    task->rflags = 0x202;
    task->rbp = task->stack_top;
    sched_enqueue(task);
    return task;
}
int sched_destroy_task(uint32_t tid) {
    task_t *task = NULL;
    for (int prio = 0; prio < NUM_PRIORITY_LEVELS; prio++) {
        for (task_t *t = kernel_scheduler.ready_queues[prio].head; t; t = t->next) {
            if (t->tid == tid) {
                task = t;
                break;
            }
        }
        if (task) break;
    }
    if (!task) {
        return -1;
    }
    if (task->state == TASK_READY) {
        sched_dequeue(task);
    }
    sched_free_stack(task->stack_base);
    mm_free(task);
    return 0;
}
void sched_yield(void) {
    task_t *prev = kernel_scheduler.current_task;
    if (prev) {
        prev->state = TASK_READY;
        prev->time_slice = prev->time_slice_max;
        sched_enqueue(prev);
    }
    task_t *next = sched_pick_next();
    if (!next) {
        return;
    }
    sched_dequeue(next);
    next->state = TASK_RUNNING;
    if (next->start_ticks == 0) {
        next->start_ticks = kernel_scheduler.system_ticks;
    }
    kernel_scheduler.current_task = next;
    kernel_scheduler.context_switches++;
    if (prev) {
        sched_context_switch(prev, next);
    } else {
        sched_restore_context(next);
    }
}
void sched_block_until(uint64_t wakeup_ticks) {
    task_t *task = kernel_scheduler.current_task;
    if (!task) return;
    task->state = TASK_BLOCKED;
    task->wakeup_time = wakeup_ticks;
    task_t *next = sched_pick_next();
    if (next) {
        sched_dequeue(next);
        next->state = TASK_RUNNING;
        if (next->start_ticks == 0) {
            next->start_ticks = kernel_scheduler.system_ticks;
        }
        kernel_scheduler.current_task = next;
        kernel_scheduler.context_switches++;
        sched_context_switch(task, next);
    }
}
void sched_unblock_task(task_t *task) {
    if (!task || task->state != TASK_BLOCKED) return;
    task->state = TASK_READY;
    task->wakeup_time = 0;
    sched_enqueue(task);
}
int sched_set_priority(uint32_t tid, uint32_t new_priority) {
    if (new_priority >= NUM_PRIORITY_LEVELS) {
        return -1;
    }
    task_t *task = kernel_scheduler.current_task;
    if (!task || task->tid != tid) {
        return -1;
    }
    task->priority = new_priority;
    task->base_priority = new_priority;
    if (task->state == TASK_READY && task != kernel_scheduler.current_task) {
        sched_dequeue(task);
        sched_enqueue(task);
    }
    return 0;
}
void sched_tick(void) {
    kernel_scheduler.system_ticks++;
    task_t *current = kernel_scheduler.current_task;
    if (!current) {
        return;
    }
    current->time_slice--;
    current->total_ticks++;
    for (int prio = 0; prio < NUM_PRIORITY_LEVELS; prio++) {
        task_t *task = kernel_scheduler.ready_queues[prio].head;
        while (task) {
            task_t *next = task->next;
            if (task->state == TASK_BLOCKED &&
                task->wakeup_time > 0 &&
                kernel_scheduler.system_ticks >= task->wakeup_time) {
                sched_dequeue(task);
                task->state = TASK_READY;
                sched_enqueue(task);
            }
            task = next;
        }
    }
    int should_preempt = 0;
    if (current->time_slice == 0) {
        should_preempt = 1;
    } else if (kernel_scheduler.highest_priority_ready > current->priority) {
        should_preempt = 1;
    }
    if (should_preempt) {
        kernel_scheduler.preemptions++;
        current->state = TASK_READY;
        current->time_slice = current->time_slice_max;
        sched_enqueue(current);
        task_t *next = sched_pick_next();
        if (next && next != current) {
            sched_dequeue(next);
            next->state = TASK_RUNNING;
            if (next->start_ticks == 0) {
                next->start_ticks = kernel_scheduler.system_ticks;
            }
            kernel_scheduler.current_task = next;
            kernel_scheduler.context_switches++;
            sched_context_switch(current, next);
        }
    }
    sched_update_highest_priority();
}