#include "kernel_memory/listeners.h"

void* listen_scheduler(void* ptr)
{
  t_scheduler_data* scheduler_data = (t_scheduler_data*)ptr;
  bool connection_alive = true;
  while (connection_alive)
  {
    switch (receive_op_code(scheduler_data->socket_scheduler))
    {
      case OP_NEW_PROCESS:
      {
        t_list* packet = receive_packet(scheduler_data->socket_scheduler);
        char* relative_path = list_get(packet, 0);
        u_int32_t* pid = list_get(packet, 1);

        t_process* process =
            init_process(*pid, relative_path, scheduler_data->scripts_basepath,
                         scheduler_data->logger);

        list_add_mtx(scheduler_data->processes, scheduler_data->processes_mutex,
                     process);

        log_info(scheduler_data->logger, "PID: %d  - Process created", *pid);
        list_destroy_and_destroy_elements(packet, free);
        send_string(OP_PROCESS_STARTED, "Process created",
                    scheduler_data->socket_scheduler);
        break;
      }
      case OP_CREATE_SEGMENT:
      {
        log_debug(scheduler_data->logger, "Received a MEM_ALLOC request");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)receive_buffer(
            &a, scheduler_data->socket_scheduler);

        if (syscall->size > scheduler_data->main_memory->max_segment_size)
        {
          log_debug(
              scheduler_data->logger,
              "The MEM_ALLOC syscall failed because the requested size is "
              "larger than the max segment "
              "size than the max segment size");
          send_string(OP_SEGMENT_SIZE_EXCEEDED,
                      "Requested size is larger than the max segment size",
                      scheduler_data->socket_scheduler);
        }
        else
        {
          create_segment(syscall->segment_id, syscall->pid, syscall->size,
                         scheduler_data->main_memory,
                         scheduler_data->socket_scheduler,
                         scheduler_data->logger);
        }
        free(syscall);
        break;
      }
      case OP_DELETE_SEGMENT:
      {
        log_debug(scheduler_data->logger, "Received a MEM_FREE request");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)receive_buffer(
            &a, scheduler_data->socket_scheduler);
        remove_segment(syscall->segment_id, syscall->pid,
                       scheduler_data->main_memory, scheduler_data->logger);
        log_trace(scheduler_data->logger, "Segment removed");
        free(syscall);
        send_string(OP_MEMORY_FREED, "Memory freed",
                    scheduler_data->socket_scheduler);
        break;
      }
      case OP_IO_STDIN_REQUEST:
      {
        log_debug(scheduler_data->logger, "Received a STDIN request");
        t_list* stdin_packet = receive_packet(scheduler_data->socket_scheduler);
        t_stdin_request* peticion_stdin =
            (t_stdin_request*)list_get(stdin_packet, 0);
        char* write_buffer = (char*)list_get(stdin_packet, 1);
        int physical_address = translate_logical_address(
            peticion_stdin->pid, peticion_stdin->logical_address,
            peticion_stdin->bytes_to_read, scheduler_data->main_memory,
            scheduler_data->logger);
        if (physical_address == -1)
        {
          send_string(OP_STDIN_RESPONSE, "Segmentation Fault",
                      scheduler_data->socket_scheduler);
          list_destroy_and_destroy_elements(stdin_packet, free);
          break;
        }
        log_info(scheduler_data->logger,
                 "PID: %u - Write - "
                 "Phys. Addr: %u - Size: %d",
                 peticion_stdin->pid, physical_address,
                 peticion_stdin->bytes_to_read);

        int requested_size = peticion_stdin->bytes_to_read;
        char* safe_buffer = calloc(requested_size, sizeof(char));

        size_t valid_bytes = strnlen(write_buffer, requested_size);
        memcpy(safe_buffer, write_buffer, valid_bytes);

        if (!write_to_sticks(
                peticion_stdin->pid, physical_address, requested_size,
                safe_buffer, scheduler_data->connected_sticks,
                scheduler_data->socket_list_mutex, scheduler_data->logger,
                scheduler_data->socket_scheduler))
        {
          log_warning(scheduler_data->logger, "Error writing to sticks");
          connection_alive = false;
          free(safe_buffer);
          list_destroy_and_destroy_elements(stdin_packet, free);
          break;
        }
        free(safe_buffer);
        send_string(OP_STDIN_RESPONSE, "Memory written",
                    scheduler_data->socket_scheduler);
        log_trace(scheduler_data->logger, "STDIN request finished");
        list_destroy_and_destroy_elements(stdin_packet, free);
        break;
      }
      case OP_IO_STDOUT_REQUEST:
      {
        log_debug(scheduler_data->logger, "Received a STDOUT request");
        int size;
        t_stdout_request* peticion_stdout = (t_stdout_request*)receive_buffer(
            &size, scheduler_data->socket_scheduler);
        int physical_address = translate_logical_address(
            peticion_stdout->pid, peticion_stdout->logical_address,
            peticion_stdout->bytes_to_write, scheduler_data->main_memory,
            scheduler_data->logger);
        log_info(scheduler_data->logger,
                 "PID: %u - Read - "
                 "Phys. Addr: %u - Size: %d",
                 peticion_stdout->pid, physical_address,
                 peticion_stdout->bytes_to_write);
        if (physical_address == -1)
        {
          send_string(OP_STDOUT_RESPONSE, "Segmentation Fault",
                      scheduler_data->socket_scheduler);
          free(peticion_stdout);
          break;
        }
        char* buffer = read_from_sticks(
            physical_address, peticion_stdout->bytes_to_write,
            scheduler_data->connected_sticks, scheduler_data->socket_list_mutex,
            scheduler_data->logger, scheduler_data->socket_scheduler);
        if (buffer == NULL)
        {
          send_string(OP_STDOUT_RESPONSE, "Stick read error",
                      scheduler_data->socket_scheduler);
          connection_alive = false;
          free(peticion_stdout);
          break;
        }
        send_string(OP_STDOUT_RESPONSE, buffer,
                    scheduler_data->socket_scheduler);
        free(buffer);
        free(peticion_stdout);
        log_trace(scheduler_data->logger, "STDOUT request finished");
        break;
      }
      case OP_END_PROCESS:
      {
        log_debug(scheduler_data->logger, "Received an END_PROCESS request");
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, scheduler_data->socket_scheduler);
        t_process* process_to_end = find_process(
            scheduler_data->processes, scheduler_data->processes_mutex, *pid);
        t_list* segment_list = filter_process_segments(
            *pid, scheduler_data->main_memory, scheduler_data->logger);
        if (process_to_end != NULL)
        {
          pthread_mutex_lock(scheduler_data->processes_mutex);
          list_remove_element(scheduler_data->processes, process_to_end);
          pthread_mutex_unlock(scheduler_data->processes_mutex);
          log_info(scheduler_data->logger, "Process with PID %u ended", *pid);
          t_list_iterator* segment_iterator =
              list_iterator_create(segment_list);
          while (list_iterator_has_next(segment_iterator))
          {
            t_segment* segment = list_iterator_next(segment_iterator);
            remove_segment(segment->id, process_to_end->pid,
                           scheduler_data->main_memory, scheduler_data->logger);
          }
          list_iterator_destroy(segment_iterator);
          free_process(process_to_end);
        }
        else
        {
          log_debug(scheduler_data->logger,
                    "Process with PID %u to terminate was not found", *pid);
        }
        list_destroy(segment_list);
        free(pid);
        break;
      }
      case OP_REQUEST_FREE_MEMORY:
      {
        free(receive_string(scheduler_data->socket_scheduler));
        log_trace(scheduler_data->logger,
                  "The scheduler requested the available memory");

        int size =
            compute_free_space(scheduler_data->main_memory->holes,
                               scheduler_data->main_memory->main_memory_mutex,
                               scheduler_data->logger);
        log_trace(scheduler_data->logger, "free space computed");
        send_buffer(OP_FREE_MEMORY, &size, sizeof(int),
                    scheduler_data->socket_scheduler);
        log_trace(scheduler_data->logger, "free space sent");
        break;
      }
      case OP_REQUEST_PROCESS_SIZE:
      {
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, scheduler_data->socket_scheduler);
        t_process* process = find_process(
            scheduler_data->processes, scheduler_data->processes_mutex, *pid);
        int size = compute_process_size(process, scheduler_data->main_memory);
        send_buffer(OP_PROCESS_SIZE, &size, sizeof(int),
                    scheduler_data->socket_scheduler);
        free(pid);
        break;
      }
      case OP_SUSPEND_PROCESS:
      {
        log_debug(scheduler_data->logger, "Received a SUSPEND_PROCESS request");
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, scheduler_data->socket_scheduler);
        t_process* process_to_suspend = find_process(
            scheduler_data->processes, scheduler_data->processes_mutex, *pid);
        log_trace(scheduler_data->logger, "PID received: %u", *pid);

        suspend_process(process_to_suspend, scheduler_data);
        free(pid);
        break;
      }
      case OP_RESUME_SUSPENDED_PROCESS:
      {
        log_debug(scheduler_data->logger, "Received a RESUME_PROCESS request");
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, scheduler_data->socket_scheduler);
        resume_process(*pid, scheduler_data);
        free(pid);
        break;
      }
      case OP_KERNEL_SCHEDULER_SHUTDOWN:
      {
        log_debug(scheduler_data->logger,
                  "Received a request to close communications");
        connection_alive = false;
        break;
      }
      case OP_KERNEL_MEMORY_RUNNING:
        free(receive_string(scheduler_data->socket_scheduler));
        break;
      case OP_CODE_ERROR:
        connection_alive = false;
        break;
      default:
        log_error(scheduler_data->logger, "Error: unrecognized operation code");
        connection_alive = false;
        break;
    }
  }
  log_debug(scheduler_data->logger, "Scheduler listener shutting down");
  send_string(OP_MEMORY_CORRUPTED, "Kernel shutdown",
              scheduler_data->socket_scheduler);
  pthread_mutex_lock(scheduler_data->active_threads_mutex);
  (*scheduler_data->active_threads)--;
  pthread_cond_signal(scheduler_data->active_threads_cond);
  pthread_mutex_unlock(scheduler_data->active_threads_mutex);
  free_scheduler_data(scheduler_data);
  return NULL;
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

void start_scheduler_listener(t_scheduler_data* scheduler_data)
{
  pthread_t thread_escucha;
  pthread_create(&thread_escucha, NULL, listen_scheduler, scheduler_data);
  pthread_detach(thread_escucha);
}

void start_cpu_listener(t_cpu_data* cpu_data)
{
  pthread_t thread_escucha;
  pthread_create(&thread_escucha, NULL, listen_cpu, cpu_data);
  pthread_detach(thread_escucha);
}
