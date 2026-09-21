#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/blocking_list.h"  // IWYU pragma: export
#include "kernel_scheduler/scheduler/compaction.h"     // IWYU pragma: export
#include "kernel_scheduler/scheduler/counter.h"        // IWYU pragma: export
#include "kernel_scheduler/scheduler/exec_list.h"      // IWYU pragma: export
#include "kernel_scheduler/scheduler/memory_query.h"   // IWYU pragma: export
#include "kernel_scheduler/scheduler/queue_types.h"    // IWYU pragma: export
#include "kernel_scheduler/scheduler/ready_queue.h"    // IWYU pragma: export
#include "kernel_scheduler/scheduler/suspension.h"     // IWYU pragma: export
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/sockets.h"

/**
 * @brief Creates every subqueue, counter and background thread the
 *        scheduler needs.
 * @param multilevel_algorithms  NULL unless @p algorithm is
 *        SA_MULTILEVEL_QUEUES.
 * @param quantum  0 unless @p algorithm is RR.
 */
t_queues* init_queues(int algorithm, t_list* multilevel_algorithms, int quantum,
                      bool preemption, t_log* logger, t_socket* km_socket,
                      int suspension_timeout);

/** @brief Waits for every routine/worker thread to finish, then frees
 *         everything. */
void destroy_queues(t_queues* queues);

/** @brief Re-inserts @p pcb into its (possibly changed) priority subqueue,
 *         if it's in READY and the queue is multilevel. */
void update_priority(t_pcb* pcb, t_queues* queues);

/** @brief READY -> EXEC, state field only (the caller does the list
 *         bookkeeping via transition_take_ready*() / transition_to_exec()). */
void transition_ready_exec(t_pcb* pcb, t_queues* queues);

/**
 * @brief The scheduler's state-change functions, named
 *        transition_<from>_<to>: they validate the PCB's current state,
 *        move it between the matching queues/lists, and log the move (or
 *        the invalid attempt). transition_new_ready() also creates the PCB
 *        and notifies Kernel Memory; transition_unlock() picks BLOCK ->
 *        READY or SUSP. BLOCK -> SUSP. READY depending on the PCB's current
 *        state.
 */
void transition_new_ready(t_queues* queues, char* instructions_file,
                          int priority);
void transition_exec_ready(t_pcb* pcb, t_queues* queues);
/** @brief EXEC -> EXIT for @p reason; also destroys the PCB once it's safe
 *         to (waits for in-flight syscalls/IO on it first). */
void transition_exec_exit(t_pcb* pcb, t_queues* queues, int reason);
void transition_exec_block(t_pcb* pcb, t_queues* queues);
void transition_block_ready(t_pcb* pcb, t_queues* queues);
/** @brief BLOCK -> SUSP. BLOCK: asks Kernel Memory to swap the process out. */
void transition_block_susp_block(t_pcb* pcb, t_queues* queues);
/** @brief SUSP. BLOCK -> BLOCK, without going through SUSP. READY / READY. */
void transition_susp_block(t_pcb* pcb, t_queues* queues);
void transition_susp_block_susp_ready(t_pcb* pcb, t_queues* queues);
/**
 * @brief SUSP. READY -> READY, asking Kernel Memory to restore the process.
 * @return false if there isn't enough space or Kernel Memory rejects it (the
 *         PCB is left in SUSP. READY).
 */
bool transition_susp_ready(t_pcb* pcb, t_queues* queues);
void transition_unlock(t_pcb* pcb, t_queues* queues);

/** @brief Drains every non-EXIT queue, transitioning each PCB to EXIT (used
 *         on shutdown). */
void clear_queues(t_queues* queues);

/** @brief Adjusts the count of in-flight syscalls, gating the
 *         exec-empty-except-syscalls wait used before compaction. */
void increment_syscall_counter(t_queues* queues);
void decrement_syscall_counter(t_queues* queues);
