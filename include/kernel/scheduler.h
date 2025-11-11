/*
 * Process Scheduler for ThunderOS
 * 
 * Implements a round-robin scheduler with priority support.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "kernel/process.h"

/**
 * Initialize the scheduler
 */
void scheduler_init(void);

/**
 * Schedule next process to run
 * 
 * This function is called by the timer interrupt to perform
 * preemptive multitasking. It selects the next process to run
 * and performs a context switch if needed.
 */
void schedule(void);

/**
 * Voluntarily yield CPU to another process
 * 
 * Current process gives up its CPU time slice and scheduler
 * picks the next process to run. Useful for cooperative multitasking.
 */
void scheduler_yield(void);

/**
 * Add a process to the ready queue
 * 
 * @param proc Process to add
 */
void scheduler_enqueue(struct process *proc);

/**
 * Remove a process from the ready queue
 * 
 * @param proc Process to remove
 */
void scheduler_dequeue(struct process *proc);

/**
 * Perform a context switch from old process to new process
 * 
 * @param old Old process (can be NULL)
 * @param new New process
 */
void context_switch(struct process *old, struct process *new);

/**
 * Get the next process to run
 * 
 * @return Next process, or NULL if none available
 */
struct process *scheduler_pick_next(void);

#endif // SCHEDULER_H
