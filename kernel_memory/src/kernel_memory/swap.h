#pragma once

#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/protocol.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

void suspend_process(t_process* process_to_suspend,
                     t_scheduler_data* scheduler_data);
void resume_process(uint32_t pid, t_scheduler_data* scheduler_data);
