#pragma once

#include <pthread.h>
#include <stdio.h>

#include "cpu/registers.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/msg.h"
#include "utils/registros_cpu.h"

typedef struct
{
  char* id;
  int socket_kernel_memory;
  int socket_kernel_scheduler;
  uint32_t max_segment_size;

  t_list* memory_sticks;
  t_dictionary* handlers;
  t_log* logger;
  t_config* config;
} t_cpu;

typedef struct
{
  char* name;
  char* parameters[3];
  int parameter_count;
} t_instruction;

typedef struct
{
  int socket_ms;
  uint32_t size;
  uint32_t offset;
} t_memory_stick_info;

// A boolean plus the two out-of-band results the instruction cycle needs:
// EB_ERROR (fatal, abort) and EB_NO_TABLE (kernel memory reported the segment
// table is gone).
typedef enum
{
  EB_FALSE,
  EB_TRUE,
  EB_ERROR,
  EB_NO_TABLE
} t_extended_bool;

bool receive_max_segment_size(t_cpu* cpu);
bool listen_kernel_memory(t_cpu* cpu);
bool parse_stick_packet(t_cpu* cpu, t_list* packet, char stick_ip[16],
                        char stick_port[6], uint32_t* size);
void run_instruction_loop(t_cpu* cpu);
uint32_t receive_pid(t_cpu* cpu);
bool request_context(t_cpu* cpu, uint32_t pid);
t_registros* receive_context(t_cpu* cpu);
t_list* receive_segment_table(t_cpu* cpu, t_context* context);
bool run_instruction_cycle(t_cpu* cpu, uint32_t pid, t_context* context);
char* fetch_stage(t_cpu* cpu, uint32_t pid, uint32_t pc);
bool request_instruction(t_cpu* cpu, uint32_t pid, uint32_t pc);
char* receive_instruction(t_cpu* cpu);
t_instruction* decode_stage(char* raw_instruction);
t_extended_bool execute_stage(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid);
t_extended_bool check_interrupt(t_cpu* cpu, uint32_t pid);
bool send_updated_context(t_cpu* cpu, uint32_t pid, t_registros* updated_context);
bool update_segment_table(t_cpu* cpu, uint32_t pid, t_context* context);
