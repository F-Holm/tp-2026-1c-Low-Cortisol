#include "kernel_memory/swap.h"

static int find_free_block(t_swap_data* swap_data);
static int add_block_to_swap(t_segment* segment, int counter,
                             t_swap_data* swap_data, t_log* logger);
static void write_block_to_swap(int block_number, char* content, int byte_count,
                                t_swap_data* swap);
static void remove_process_segments(t_list* segments_to_remove, uint32_t pid,
                                    t_scheduler_data* scheduler_data);
static bool can_resume(uint32_t pid, t_scheduler_data* scheduler_data);
static bool regenerate_segment(uint32_t id, uint32_t pid, int size,
                               t_main_memory* main_memory, int socket_scheduler,
                               t_log* logger);
static char* read_block_from_swap(int block_number, t_swap_data* swap);
static int remove_block_from_swap(int block_number, t_swap_data* swap_data,
                                  t_log* logger);
static int compute_address_with_offset(int block_size,
                                       t_main_memory* main_memory, uint32_t pid,
                                       t_block_data* block);

void suspend_process(t_process* process_to_suspend,
                     t_scheduler_data* scheduler_data)
{
  if (process_to_suspend == NULL)
  {
    log_error(scheduler_data->logger,
              "suspend_process received a NULL process");
    return;
  }

  int block_size = scheduler_data->swap_data->block_size;
  bool process_suspended = false;
  t_list* segments_to_remove = list_create();
  t_list_iterator* iterator =
      list_iterator_create(scheduler_data->main_memory->segments);
  while (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    if (current_segment->pid == process_to_suspend->pid)
    {
      int segment_block_count =
          (current_segment->size + block_size - 1) / block_size;
      bool segment_suspended = false;
      for (int i = 0; i < segment_block_count; i++)
      {
        int offset = i * block_size;
        int bytes_to_read = (current_segment->size - offset) < block_size
                                ? (current_segment->size - offset)
                                : block_size;
        char* content = read_from_sticks(
            current_segment->base + offset, bytes_to_read,
            scheduler_data->connected_sticks, scheduler_data->socket_list_mutex,
            scheduler_data->logger, scheduler_data->socket_scheduler);
        int block_number =
            add_block_to_swap(current_segment, i, scheduler_data->swap_data,
                              scheduler_data->logger);
        if (block_number != -1)
        {
          write_block_to_swap(block_number, content, bytes_to_read,
                              scheduler_data->swap_data);
          segment_suspended = true;
        }
        else
        {
          log_info(scheduler_data->logger,
                   "Could not suspend process PID %d: not free blocks "
                   "in swap.",
                   process_to_suspend->pid);
          send_string(OP_SUSPENSION_FAILED,
                      "Could not suspend the process because swap is full.",
                      scheduler_data->socket_scheduler);
          segment_suspended = false;
          free(content);
          break;
        }
        free(content);
      }
      if (segment_suspended)
      {
        list_add(segments_to_remove, current_segment);
        process_suspended = true;
      }
      else
      {
        list_iterator_destroy(iterator);
        return;
      }
    }
  }
  list_iterator_destroy(iterator);
  remove_process_segments(segments_to_remove, process_to_suspend->pid,
                          scheduler_data);
  if (process_suspended)
  {
    log_info(scheduler_data->logger,
             "Process PID %d was suspended successfully.",
             process_to_suspend->pid);
    send_string(OP_SUSPENSION_OK, "", scheduler_data->socket_scheduler);
  }
  else
  {
    log_info(scheduler_data->logger, "Process PID %d was not found.",
             process_to_suspend->pid);
    send_string(OP_SUSPENSION_FAILED,
                "Could not suspend the process because its pid was not found.",
                scheduler_data->socket_scheduler);
  }
}

void resume_process(uint32_t pid, t_scheduler_data* scheduler_data)
{
  int block_size = scheduler_data->swap_data->block_size;
  bool process_found = false;
  if (!can_resume(pid, scheduler_data))
  {
    log_info(scheduler_data->logger,
             "Could not suspend because the process does not fit in memory");
    send_string(OP_RESUME_SUSPENSION_FAILED,
                "The process does not fit in memory",
                scheduler_data->socket_scheduler);
    return;
  }
  t_list_iterator* iterator =
      list_iterator_create(scheduler_data->swap_data->block_list);
  while (list_iterator_has_next(iterator))
  {
    t_block_data* block = list_iterator_next(iterator);
    if (block->pid == pid)
    {
      /* works because blocks of the same segment are assumed to be in
       * order due to the logic of find_free_block */
      process_found = true;
      if (block->segment_block_number == 0)
      {
        bool regen_ok = regenerate_segment(
            block->segment_number, pid, block->segment_size,
            scheduler_data->main_memory, scheduler_data->socket_scheduler,
            scheduler_data->logger);
        if (!regen_ok)
        {
          list_iterator_destroy(iterator);
          return;
        }
      }
      char* content =
          read_block_from_swap(block->block_number, scheduler_data->swap_data);
      if (content == NULL)
      {
        log_error(scheduler_data->logger,
                  "Could not read block %d from swap for PID %d",
                  block->block_number, pid);
        send_string(OP_RESUME_SUSPENSION_FAILED, "Error reading swap block",
                    scheduler_data->socket_scheduler);
        list_iterator_destroy(iterator);
        return;
      }
      int block_offset = block->segment_block_number * block_size;
      int real_bytes = (block->segment_size - block_offset) < block_size
                           ? (block->segment_size - block_offset)
                           : block_size;

      int write_address = compute_address_with_offset(
          block_size, scheduler_data->main_memory, pid, block);

      write_to_sticks(pid, write_address, real_bytes, content,
                      scheduler_data->connected_sticks,
                      scheduler_data->socket_list_mutex, scheduler_data->logger,
                      scheduler_data->socket_scheduler);
      free(content);
      if (block->block_number !=
          remove_block_from_swap(block->block_number, scheduler_data->swap_data,
                                 scheduler_data->logger))
      {
        log_info(scheduler_data->logger,
                 "Since block #%d was not found in swap, "
                 "PID %d cannot be resumed.",
                 block->block_number, pid);
        send_string(OP_RESUME_SUSPENSION_FAILED,
                    "Could not remove the swap block.",
                    scheduler_data->socket_scheduler);
        list_iterator_destroy(iterator);
        return;
      }
    }
  }
  list_iterator_destroy(iterator);
  if (!process_found)
  {
    log_info(scheduler_data->logger,
             "The process is not suspended or its blocks were not "
             "found on disk.");
    send_string(OP_RESUME_SUSPENSION_FAILED,
                "The process was not resumed because its "
                "blocks found on disk.",
                scheduler_data->socket_scheduler);
  }
  else
  {
    log_info(scheduler_data->logger, "Process PID %d was resumed successfully.",
             pid);
    send_string(OP_RESUME_SUSPENSION_OK, "", scheduler_data->socket_scheduler);
  }
}

// SUSPEND PROCESS
static int find_free_block(t_swap_data* swap_data)
{
  for (int i = 0; i < swap_data->swap_size / swap_data->block_size; i++)
  {
    t_block_data* block = list_get(swap_data->block_list, i);
    if (block->pid == -1)
      return i;
  }
  log_info(swap_data->logger, "No free blocks in swap");
  return -1;
}

static int add_block_to_swap(t_segment* segment, int counter,
                             t_swap_data* swap_data, t_log* logger)
/* returns the added block number, or -1 if it could not be added */
{
  int free_block_number = find_free_block(swap_data);
  if (free_block_number != -1)
  {
    t_block_data* free_block =
        list_get(swap_data->block_list, free_block_number);
    free_block->segment_number = segment->id;
    free_block->segment_block_number = counter;
    free_block->pid = segment->pid;
    free_block->segment_size = segment->size;
    log_info(logger,
             "Adding block to swap: PID %d, Segment %d, segment block %d.",
             free_block->pid, free_block->segment_number,
             free_block->segment_block_number);
  }
  else
  {
    log_info(logger, "Cannot add the segment to swap: no free blocks.");
  }
  return free_block_number;
}

static void write_block_to_swap(int block_number, char* content, int byte_count,
                                t_swap_data* swap)
{
  t_packet* packet = create_packet(OP_DISK_WRITE);
  packet_append(packet, &block_number, sizeof(int));
  char* aux_buffer = calloc(swap->block_size, 1);
  memcpy(aux_buffer, content, byte_count);
  packet_append(packet, aux_buffer, swap->block_size);
  send_packet(packet, swap->socket_swap);
  destroy_packet(packet);
  free(aux_buffer);

  int op_code = receive_op_code(swap->socket_swap);
  if (op_code == OP_DISK_WRITE_DONE)
  {
    char* ack = receive_string(swap->socket_swap);
    free(ack);
  }
  else
  {
    log_error(swap->logger,
              "Unexpected response from the swap module while writing block %d",
              block_number);
  }
}

static void remove_process_segments(t_list* segments_to_remove, uint32_t pid,
                                    t_scheduler_data* scheduler_data)
{
  t_list_iterator* it = list_iterator_create(segments_to_remove);
  while (list_iterator_has_next(it))
  {
    t_segment* seg = list_iterator_next(it);
    remove_segment(seg->id, pid, scheduler_data->main_memory,
                   scheduler_data->logger);
  }
  list_iterator_destroy(it);
  list_destroy(segments_to_remove);
}

// RESUME PROCESS
static bool can_resume(uint32_t pid, t_scheduler_data* scheduler_data)
{
  int process_size = 0;
  t_list* blocks = scheduler_data->swap_data->block_list;
  for (int i = 0; i < list_size(blocks); i++)
  {
    t_block_data* block = list_get(blocks, i);
    if (block->pid == pid && block->segment_block_number == 0)
    {
      process_size += block->segment_size;
    }
  }
  pthread_mutex_lock(scheduler_data->main_memory->main_memory_mutex);
  int free_space = compute_free_space(
      scheduler_data->main_memory->holes,
      scheduler_data->main_memory->main_memory_mutex, scheduler_data->logger);
  pthread_mutex_unlock(scheduler_data->main_memory->main_memory_mutex);
  log_info(scheduler_data->logger,
           "PID %u: size required to resume %d, free space %d", pid,
           process_size, free_space);
  return process_size <= free_space;
}

static bool regenerate_segment(uint32_t id, uint32_t pid, int size,
                               t_main_memory* main_memory, int socket_scheduler,
                               t_log* logger)
{
  // Check available memory
  pthread_mutex_lock(main_memory->main_memory_mutex);
  if (compute_free_space(main_memory->holes, main_memory->main_memory_mutex,
                         logger) < size)
  {
    log_info(logger, "Not enough space to regenerate the segment");
    send_string(OP_RESUME_SUSPENSION_FAILED, "There are not memory enough",
                socket_scheduler);
    pthread_mutex_unlock(main_memory->main_memory_mutex);
    return false;
  }
  else
  {
    log_info(logger, "Regenerating segment with id %u, pid %u and size %d", id,
             pid, size);
    t_hole chosen_hole = select_hole(size, logger, main_memory);

    // Compaction check
    if (chosen_hole.size == -1)
    {
      log_error(logger, "## Could not allocate any hole.");
      send_string(OP_RESUME_SUSPENSION_FAILED, "Could not allocate holes.",
                  socket_scheduler);
      return false;
    }
    update_segment_list(main_memory, chosen_hole, size, pid, id);
    pthread_mutex_unlock(main_memory->main_memory_mutex);
    log_info(logger, "## PID: %u - Segment regenerated %u - Size: %d", pid, id,
             size);
    return true;
  }
}

static char* read_block_from_swap(int block_number, t_swap_data* swap)
{
  send_buffer(OP_DISK_READ, &block_number, sizeof(int), swap->socket_swap);

  int op_code = receive_op_code(swap->socket_swap);
  if (op_code != OP_DISK_READ_DONE)
  {
    log_error(swap->logger,
              "Unexpected response from the swap module while reading block %d",
              block_number);
    return NULL;
  }

  int a;
  char* read_content = (char*)receive_buffer(&a, swap->socket_swap);
  return read_content;
}

static int remove_block_from_swap(int block_number, t_swap_data* swap_data,
                                  t_log* logger)
/* returns the removed block number, or -1 if it could not be removed */
{
  t_block_data* block_to_remove = list_get(swap_data->block_list, block_number);
  if (block_to_remove == NULL)
  {
    log_info(logger,
             "Could not remove swap block number %d because "
             "there is not such block.",
             block_number);
    return -1;
  }
  block_to_remove->segment_number = -1;
  block_to_remove->segment_block_number = -1;
  block_to_remove->pid = -1;
  block_to_remove->segment_size = -1;
  log_info(logger, "Removing swap block number: %d.", block_number);
  return block_number;
}

static int compute_address_with_offset(int block_size,
                                       t_main_memory* main_memory, uint32_t pid,
                                       t_block_data* block)
{
  t_segment* segment = find_segment(main_memory, pid, block->segment_number);
  return (segment->base + block_size * block->segment_block_number);
}
