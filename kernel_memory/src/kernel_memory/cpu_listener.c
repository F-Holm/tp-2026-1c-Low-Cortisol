#include "kernel_memory/cpu_listener.h"

void* listen_cpu(void* ptr)
{
  t_cpu_data* cpu_data = (t_cpu_data*)ptr;
  bool connection_alive = true;
  while (connection_alive)
  {
    switch (receive_op_code(cpu_data->socket_cpu))
    {
      case OP_NEXT_INSTRUCTION:
      {
        t_list* packet = receive_packet(cpu_data->socket_cpu);
        uint32_t pid = *(uint32_t*)list_get(packet, 0);
        uint32_t pc = *(uint32_t*)list_get(packet, 1);

        t_process* process =
            find_process(cpu_data->processes, cpu_data->processes_mutex, pid);
        char* instruction = process->instructions[pc];
        log_info(cpu_data->logger,
                 "PID: %u - Get instruction: %u - Instruction: %s", pid, pc,
                 instruction);
        usleep(cpu_data->instruction_delay * 1000);
        send_string(OP_SEND_INSTRUCTION, instruction, cpu_data->socket_cpu);
        list_destroy_and_destroy_elements(packet, free);

        break;
      }
      case OP_REQUEST_CONTEXT:
      {
        int a;
        uint32_t* pid = (uint32_t*)receive_buffer(&a, cpu_data->socket_cpu);
        t_process* process =
            find_process(cpu_data->processes, cpu_data->processes_mutex, *pid);
        if (process == NULL)
        {
          log_error(cpu_data->logger, "Process with pid %d not found", *pid);
          free(pid);
          break;
        }
        log_trace(cpu_data->logger, "PID: %d - Get registers", *pid);
        log_trace(cpu_data->logger, "instruction delay %d",
                  cpu_data->instruction_delay);
        usleep(cpu_data->instruction_delay * 1000);
        send_buffer(OP_SEND_CONTEXT, &process->registers, sizeof(t_registers),
                    cpu_data->socket_cpu);
        log_trace(cpu_data->logger, "Sending the segment table");
        t_packet* process_segment_table = create_packet(OP_SEGMENT_TABLE);
        t_list* segment_list = filter_process_segments(
            *pid, cpu_data->main_memory, cpu_data->logger);

        add_segments_to_packet(segment_list, process_segment_table);

        list_destroy(segment_list);
        send_packet(process_segment_table, cpu_data->socket_cpu);
        log_trace(cpu_data->logger, "Segment table sent");
        destroy_packet(process_segment_table);
        free(pid);
        break;
      }
      case OP_UPDATED_CONTEXT:
      {
        t_list* packet = receive_packet(cpu_data->socket_cpu);
        uint32_t pid = *(uint32_t*)list_get(packet, 0);
        t_registers registers = *(t_registers*)list_get(packet, 1);
        t_process* process =
            find_process(cpu_data->processes, cpu_data->processes_mutex, pid);
        if (process != NULL)
        {
          pthread_mutex_lock(cpu_data->processes_mutex);
          process->registers = registers;
          pthread_mutex_unlock(cpu_data->processes_mutex);
        }
        list_destroy_and_destroy_elements(packet, free);
        break;
      }
      case OP_UPDATED_SEGMENT_TABLE:
      {
        log_trace(cpu_data->logger, "CPU needs to update the segment table");
        int a;
        uint32_t* pid = (uint32_t*)receive_buffer(&a, cpu_data->socket_cpu);
        t_process* process =
            find_process(cpu_data->processes, cpu_data->processes_mutex, *pid);
        if (process == NULL)
        {
          log_debug(cpu_data->logger,
                    "Process with PID %u to terminate was not found", *pid);
          free(pid);
          break;
        }
        log_trace(cpu_data->logger, "Sending the segment table to the CPU : %d",
                  cpu_data->id);
        t_packet* process_segment_table = create_packet(OP_SEGMENT_TABLE);
        t_list* segment_list = filter_process_segments(
            *pid, cpu_data->main_memory, cpu_data->logger);

        add_segments_to_packet(segment_list, process_segment_table);

        list_destroy(segment_list);
        send_packet(process_segment_table, cpu_data->socket_cpu);
        log_trace(cpu_data->logger, "Segment table sent to the CPU");
        destroy_packet(process_segment_table);
        free(pid);
        break;
      }
      case OP_STICK_DISCONNECTED:
        log_warning(cpu_data->logger,
                    "Notifying the Kernel Scheduler that memory is corrupted");
        if (!send_string(OP_MEMORY_CORRUPTED, "Corrupted memory",
                         cpu_data->socket_scheduler))
        {
          log_error(cpu_data->logger,
                    "Could not send the BSOD to the Kernel Scheduler");
        }
        shutdown(cpu_data->socket_scheduler, SHUT_RDWR);
      case OP_CODE_ERROR:
      default:
        connection_alive = false;
        break;
    }
  }
  close_cpu(cpu_data);
  pthread_mutex_lock(cpu_data->active_threads_mutex);
  (*cpu_data->active_threads)--;
  pthread_cond_signal(cpu_data->active_threads_cond);
  pthread_mutex_unlock(cpu_data->active_threads_mutex);
  return NULL;
}

void start_cpu_listener(t_cpu_data* cpu_data)
{
  pthread_t listener_thread;
  pthread_create(&listener_thread, NULL, listen_cpu, cpu_data);
  pthread_detach(listener_thread);
}
