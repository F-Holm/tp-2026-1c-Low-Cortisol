#pragma once

#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/protocol.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

/**
 * @brief Moves every segment of @p process_to_suspend to swap, replying
 *        OP_SUSPENSION_OK/FAILED to the scheduler.
 * @note Fails gracefully (no crash) if @p process_to_suspend is NULL or the
 *       Swap module isn't connected yet.
 */
void suspend_process(t_process* process_to_suspend,
                     t_scheduler_data* scheduler_data);

/**
 * @brief Restores @p pid's suspended segments from swap, replying
 *        OP_RESUME_SUSPENSION_OK/FAILED to the scheduler.
 * @note Fails gracefully if the Swap module isn't connected yet, or if the
 *       process doesn't fit back in memory.
 */
void resume_process(uint32_t pid, t_scheduler_data* scheduler_data);
