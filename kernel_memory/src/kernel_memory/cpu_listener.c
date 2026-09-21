#include "kernel_memory/cpu_listener.h"

#include "utils/mutex.h"
#include "utils/threads.h"
#include "utils/time.h"

// OP_REQUEST_CONTEXT and OP_UPDATED_SEGMENT_TABLE both need to hand the CPU
// pid's current segment table -- the only difference is the log wording.
static void send_segment_table(t_cpu_data* cpu_data, uint32_t pid)
{
  t_packet* process_segment_table = create_packet(OP_SEGMENT_TABLE);
  t_list* segment_list =
      filter_process_segments(pid, cpu_data->main_memory, cpu_data->logger);

  add_segments_to_packet(segment_list, process_segment_table);

  list_destroy(segment_list);
  send_packet(process_segment_table, cpu_data->socket_cpu);
  destroy_packet(process_segment_table);
}

static bool handle_next_instruction(t_cpu_data* cpu_data)
{
  t_list* packet = receive_packet(cpu_data->socket_cpu);
  uint32_t pid = *(uint32_t*)list_get(packet, 0);
  uint32_t pc = *(uint32_t*)list_get(packet, 1);

  t_process* process =
      find_process(cpu_data->processes, cpu_data->processes_mutex, pid);
  char* instruction = process->instructions[pc];
  log_info(cpu_data->logger, "PID: %u - Get instruction: %u - Instruction: %s",
           pid, pc, instruction);
  time_sleep_ms(cpu_data->instruction_delay);
  send_string(OP_SEND_INSTRUCTION, instruction, cpu_data->socket_cpu);
  list_destroy_and_destroy_elements(packet, free);
  return true;
}

static bool handle_request_context(t_cpu_data* cpu_data)
{
  int a;
  uint32_t* pid = (uint32_t*)receive_buffer(&a, cpu_data->socket_cpu);
  t_process* process =
      find_process(cpu_data->processes, cpu_data->processes_mutex, *pid);
  if (process == NULL)
  {
    log_error(cpu_data->logger, "Process with pid %d not found", *pid);
    free(pid);
    return true;
  }
  log_trace(cpu_data->logger, "PID: %d - Get registers", *pid);
  log_trace(cpu_data->logger, "Instruction delay %d",
            cpu_data->instruction_delay);
  time_sleep_ms(cpu_data->instruction_delay);
  send_buffer(OP_SEND_CONTEXT, &process->registers, sizeof(t_registers),
              cpu_data->socket_cpu);
  log_trace(cpu_data->logger, "Sending the segment table");
  send_segment_table(cpu_data, *pid);
  log_trace(cpu_data->logger, "Segment table sent");
  free(pid);
  return true;
}

static bool handle_updated_context(t_cpu_data* cpu_data)
{
  t_list* packet = receive_packet(cpu_data->socket_cpu);
  uint32_t pid = *(uint32_t*)list_get(packet, 0);
  t_registers registers = *(t_registers*)list_get(packet, 1);
  t_process* process =
      find_process(cpu_data->processes, cpu_data->processes_mutex, pid);
  if (process != NULL)
  {
    mtx_lock(cpu_data->processes_mutex);
    process->registers = registers;
    mtx_unlock(cpu_data->processes_mutex);
  }
  list_destroy_and_destroy_elements(packet, free);
  return true;
}

static bool handle_updated_segment_table(t_cpu_data* cpu_data)
{
  log_trace(cpu_data->logger, "CPU needs to update the segment table");
  int a;
  uint32_t* pid = (uint32_t*)receive_buffer(&a, cpu_data->socket_cpu);
  t_process* process =
      find_process(cpu_data->processes, cpu_data->processes_mutex, *pid);
  if (process == NULL)
  {
    log_debug(cpu_data->logger, "Process with PID %u was not found", *pid);
    free(pid);
    return true;
  }
  log_trace(cpu_data->logger, "Sending the segment table to CPU %d",
            cpu_data->id);
  send_segment_table(cpu_data, *pid);
  log_trace(cpu_data->logger, "Segment table sent to the CPU");
  free(pid);
  return true;
}

static bool handle_stick_disconnected(t_cpu_data* cpu_data)
{
  log_warning(cpu_data->logger,
              "Notifying the Kernel Scheduler that memory is corrupted");
  if (!send_string(OP_MEMORY_CORRUPTED, "Corrupted memory",
                   cpu_data->socket_scheduler))
  {
    log_error(cpu_data->logger,
              "Could not send the BSOD to the Kernel Scheduler");
  }
  socket_shutdown(cpu_data->socket_scheduler, SOCKET_SHUTDOWN_BOTH);
  return false;
}

void* listen_cpu(void* ptr)
{
  t_cpu_data* cpu_data = (t_cpu_data*)ptr;
  bool connection_alive = true;
  while (connection_alive)
  {
    switch (receive_op_code(cpu_data->socket_cpu))
    {
      case OP_NEXT_INSTRUCTION:
        connection_alive = handle_next_instruction(cpu_data);
        break;
      case OP_REQUEST_CONTEXT:
        connection_alive = handle_request_context(cpu_data);
        break;
      case OP_UPDATED_CONTEXT:
        connection_alive = handle_updated_context(cpu_data);
        break;
      case OP_UPDATED_SEGMENT_TABLE:
        connection_alive = handle_updated_segment_table(cpu_data);
        break;
      case OP_STICK_DISCONNECTED:
        connection_alive = handle_stick_disconnected(cpu_data);
        break;
      case OP_CODE_ERROR:
      default:
        connection_alive = false;
        break;
    }
  }
  close_cpu(cpu_data);
  mtx_lock(cpu_data->active_threads_mutex);
  (*cpu_data->active_threads)--;
  cnd_signal(cpu_data->active_threads_cond);
  mtx_unlock(cpu_data->active_threads_mutex);
  return NULL;
}

void start_cpu_listener(t_cpu_data* cpu_data)
{
  thrd_t listener_thread;
  thrd_create(&listener_thread, listen_cpu, cpu_data);
  thrd_detach(listener_thread);
}
