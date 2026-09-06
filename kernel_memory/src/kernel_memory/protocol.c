#include "kernel_memory/protocol.h"

void list_add_mtx(t_list* list, pthread_mutex_t* mutex, void* element)
{
  pthread_mutex_lock(mutex);
  list_add(list, element);
  pthread_mutex_unlock(mutex);
}

bool receive_cpu_id(t_cpu_data* cpu_data)
{
  if (receive_op_code(cpu_data->socket_cpu) == OP_ID_CPU)
  {
    char* id_cpu = receive_string(cpu_data->socket_cpu);
    log_info(cpu_data->logger, "## CPU %s Conectada", id_cpu);
    cpu_data->id = atoi(id_cpu);
    free(id_cpu);
    return true;
  }
  else
  {
    log_info(cpu_data->logger,
             "Could not connect to the CPU because it could not "
             "sent the ID operation");
    close_communication(cpu_data->socket_cpu);
    return false;
  }
  return false;
}

bool receive_stick_size(t_stick_data* stick_data)
{
  if (receive_op_code(stick_data->socket_stick) == OP_MEMORY_SIZE)
  {
    char* size = receive_string(stick_data->socket_stick);
    log_info(stick_data->logger, "## Memory Stick of %s bytes Conectada", size);
    stick_data->stick_size = atoi(size);
    free(size);
    return true;
  }
  else
  {
    log_info(stick_data->logger,
             "Could not connect to the stick because it could not "
             "sent the size operation");
    close_communication(stick_data->socket_stick);
    return false;
  }
  return false;
}

bool receive_stick_listen_port(t_stick_data* stick_data)
{
  if (receive_op_code(stick_data->socket_stick) == OP_PORT)
  {
    char* port = receive_string(stick_data->socket_stick);
    log_info(stick_data->logger, "Memory Stick port received %s", port);
    stick_data->stick_port = atoi(port);
    free(port);
    return true;
  }
  else
  {
    log_info(stick_data->logger,
             "Could not connect to the stick because it could not "
             "sent the port operation");
    close_communication(stick_data->socket_stick);
    return false;
  }
  return false;
}

void add_stick_connection(t_kernel_memory_data* kernel_data,
                          t_stick_data* stick_data)
{
  list_add_mtx(kernel_data->connected_sticks, kernel_data->socket_list_mutex,
               stick_data);
  return;
}

void add_cpu_connection(t_kernel_memory_data* kernel_data, t_cpu_data* cpu_data)
{
  list_add_mtx(kernel_data->connected_cpus, kernel_data->socket_list_mutex,
               cpu_data);
  return;
}

void send_connected_sticks(t_list* connected_sticks,
                           pthread_mutex_t* socket_list_mutex,
                           t_cpu_data* cpu_data)
{
  pthread_mutex_lock(socket_list_mutex);
  int total_sticks = list_size(connected_sticks);

  for (int i = 0; i < total_sticks; i++)
  {
    t_packet* packet = create_packet(OP_PACKET);
    t_stick_data* current_stick = (t_stick_data*)list_get(connected_sticks, i);

    char port[6];
    snprintf(port, sizeof(port), "%u", current_stick->stick_port);

    packet_append_string(packet, current_stick->ip_memory_stick);
    packet_append_string(packet, port);
    packet_append(packet, &current_stick->stick_size, sizeof(int));

    send_packet(packet, cpu_data->socket_cpu);
    destroy_packet(packet);
  }
  pthread_mutex_unlock(socket_list_mutex);
}

void send_cpu_connection(t_stick_data* stick_data, t_list* connected_cpus)
{
  if (list_is_empty(connected_cpus))
  {
    return;
  }
  t_packet* packet = create_packet(OP_PACKET);

  packet_append_string(packet, stick_data->ip_memory_stick);

  char port[6];
  snprintf(port, sizeof(port), "%u", stick_data->stick_port);
  packet_append_string(packet, port);
  packet_append(packet, &stick_data->stick_size, sizeof(int));

  for (int i = 0; i < list_size(connected_cpus); i++)
  {
    t_cpu_data* current_cpu = (t_cpu_data*)list_get(connected_cpus, i);
    send_packet(packet, current_cpu->socket_cpu);
  }
  destroy_packet(packet);
}

int compute_total_memory(t_list* connected_sticks,
                         pthread_mutex_t* socket_list_mutex)
{
  int total = 0;
  for (int i = 0; i < list_size(connected_sticks); i++)
  {
    pthread_mutex_lock(socket_list_mutex);
    t_stick_data* current_stick = (t_stick_data*)list_get(connected_sticks, i);
    pthread_mutex_unlock(socket_list_mutex);
    total += current_stick->stick_size;
  }
  return total;
}

int compute_free_space(t_list* holes, pthread_mutex_t* holes_mutex,
                       t_log* logger)
{
  int total = 0;
  log_info(logger, "Calculating holes...");
  t_list_iterator* iterator = list_iterator_create(holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    total += current_hole->size;
  }
  list_iterator_destroy(iterator);
  log_info(logger, "%d free space", total);
  return total;
}

t_process* find_process(t_list* process_list, pthread_mutex_t* processes_mutex,
                        uint32_t pid)
{
  pthread_mutex_lock(processes_mutex);
  t_list_iterator* iterator = list_iterator_create(process_list);
  t_process* process_found = NULL;
  while (list_iterator_has_next(iterator))
  {
    t_process* process = list_iterator_next(iterator);
    if (process->pid == pid)
    {
      process_found = process;
      break;
    }
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(processes_mutex);
  return process_found;
}

t_main_memory* add_total_memory(t_main_memory* main_memory, int memory_total)
{
  pthread_mutex_lock(main_memory->main_memory_mutex);
  int new_base = main_memory->total_size;
  main_memory->total_size += memory_total;

  t_hole* hole_contiguo = NULL;
  t_list_iterator* iterator = list_iterator_create(main_memory->holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* h = list_iterator_next(iterator);
    if (h->base + h->size == new_base)
    {
      hole_contiguo = h;
      break;
    }
  }
  list_iterator_destroy(iterator);

  if (hole_contiguo != NULL)
  {
    hole_contiguo->size += memory_total;
  }
  else
  {
    t_hole* new_hole = malloc(sizeof(t_hole));
    new_hole->base = new_base;
    new_hole->size = memory_total;
    list_add(main_memory->holes, new_hole);
  }

  pthread_mutex_unlock(main_memory->main_memory_mutex);
  return main_memory;
}

static t_hole hole_selection_algorithm(
    uint32_t size, t_list* current_holes, t_log* logger,
    t_allocation_strategy allocation_strategy)
{
  t_hole chosen_hole = {-1, -1};
  t_list_iterator* iterator = list_iterator_create(current_holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    if (current_hole->size >= size)
    {
      switch (allocation_strategy)
      {
        case BEST:
          if (chosen_hole.size == -1 || chosen_hole.size > current_hole->size)
            chosen_hole = *current_hole;
          break;
        case WORST:
          if (chosen_hole.size == -1 || chosen_hole.size < current_hole->size)
            chosen_hole = *current_hole;
          break;
      }
    }
  }
  list_iterator_destroy(iterator);
  if (chosen_hole.size == -1)
  {
    log_info(logger, "There are not holes availables");
  }
  return chosen_hole;
}

static void update_hole_table(t_main_memory* main_memory, t_hole chosen_hole,
                              uint32_t size)
{
  t_list_iterator* iterator = list_iterator_create(main_memory->holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    if (current_hole->base == chosen_hole.base)
    {
      current_hole->size -= size;
      current_hole->base += size;
      if (current_hole->size == 0)
      {
        list_iterator_remove(iterator);
        free(current_hole);
      }
      break;
    }
  }
  list_iterator_destroy(iterator);
}

t_hole select_hole(uint32_t size, t_log* logger, t_main_memory* memory)
{
  t_hole chosen_hole = {-1, -1};
  if (memory->allocation_strategy == BEST)
    chosen_hole = hole_selection_algorithm(size, memory->holes, logger, BEST);
  else if (memory->allocation_strategy == WORST)
    chosen_hole = hole_selection_algorithm(size, memory->holes, logger, WORST);
  else
  {
    log_error(logger,
              "The chosen hole-selection option "
              "is not valid.");
    return chosen_hole;
  }
  update_hole_table(memory, chosen_hole, size);
  return chosen_hole;
}

void update_segment_list(t_main_memory* main_memory, t_hole chosen_hole,
                         int size, uint32_t pid, uint32_t id)
{
  t_segment* segment = malloc(sizeof(t_segment));
  segment->base = chosen_hole.base;
  segment->pid = pid;
  segment->id = id;
  segment->size = size;
  list_add(main_memory->segments, segment);
}

void create_segment(uint32_t id, uint32_t pid, int size,
                    t_main_memory* main_memory, int socket_scheduler,
                    t_log* logger)
{
  // Chequeo of segmentation fault
  if (size > main_memory->max_segment_size)
    send_string(OP_SEGMENT_SIZE_EXCEEDED,
                "The requested size exceeds "
                "the max segment size.",
                socket_scheduler);

  // Chequeo of count of memory available
  pthread_mutex_lock(main_memory->main_memory_mutex);
  if (compute_free_space(main_memory->holes, main_memory->main_memory_mutex,
                         logger) < size)
  {
    log_info(logger, "Not enough space to create the segment");
    send_string(OP_NOT_ENOUGH_MEMORY, "There are not memory enough",
                socket_scheduler);
    pthread_mutex_unlock(main_memory->main_memory_mutex);
  }
  else
  {
    log_info(logger, "Creating segment with id %u, pid %u and size %d", id, pid,
             size);
    t_hole chosen_hole = select_hole(size, logger, main_memory);

    // Chequeo of compaction
    if (chosen_hole.size == -1)
    {
      log_info(logger, "Memory needs to be compacted");
      notify_compaction(socket_scheduler);
      compact_memory(socket_scheduler, main_memory);
      chosen_hole = select_hole(size, logger, main_memory);
    }
    update_segment_list(main_memory, chosen_hole, size, pid, id);
    pthread_mutex_unlock(main_memory->main_memory_mutex);
    send_string(OP_MEMORY_ALLOCATED, "Memory allocated", socket_scheduler);
    log_info(logger, "## PID: %u - Segment created %u - Size: %d", pid, id,
             size);
  }
}

bool compact_memory(int socket_scheduler, t_main_memory* main_memory)
{
  compact_segments(main_memory->segments);
  list_destroy_and_destroy_elements(main_memory->holes, free);
  main_memory->holes = compact_holes(
      main_memory->total_size, compute_last_segment_end(main_memory->segments));
  usleep(main_memory->compaction_delay * 1000);
  send_string(OP_COMPACTION_DONE, "Compaction finished", socket_scheduler);
  return true;
}

void compact_segments(t_list* segments)
{
  t_list_iterator* iterator = list_iterator_create(segments);

  if (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    current_segment->base = 0;

    while (list_iterator_has_next(iterator))
    {
      t_segment* next_segment = list_iterator_next(iterator);

      next_segment->base = current_segment->base + current_segment->size;

      current_segment = next_segment;
    }
  }
  list_iterator_destroy(iterator);
}

int compute_last_segment_end(t_list* segments)
{
  t_segment* ultimo_segment = list_get(segments, list_size(segments) - 1);
  return ultimo_segment->base + ultimo_segment->size;
}

t_list* compact_holes(int memory_total, int base_final_segment)
{
  t_list* holes = list_create();
  t_hole* hole_final = malloc(sizeof(t_hole));
  hole_final->base = base_final_segment;
  hole_final->size = memory_total - base_final_segment;
  list_add(holes, hole_final);
  return holes;
}

void notify_compaction(int socket_scheduler)
{
  send_string(OP_COMPACTION_NEEDED, "Memory needs to be compacted",
              socket_scheduler);
  int op_code = receive_op_code(socket_scheduler);
  if (op_code == OP_CAN_COMPACT)
  {
    char* message = receive_string(socket_scheduler);
    free(message);
  }
}

t_segment* find_and_remove_segment(uint32_t id, uint32_t pid,
                                   t_main_memory* main_memory, t_log* logger)
{
  pthread_mutex_lock(main_memory->main_memory_mutex);

  log_info(logger, "iterating segment list of size %d:",
           list_size(main_memory->segments));

  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  t_segment* found_segment = NULL;

  while (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    if (current_segment->id == id && current_segment->pid == pid)
    {
      list_iterator_remove(iterator);
      found_segment = current_segment;
      log_info(logger, "removed the segment with ID: %d, PID: %d",
               found_segment->id, found_segment->pid);
      break;
    }
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  if (found_segment != NULL)
  {
    return found_segment;
  }
  log_error(logger,
            "an error occurred while removing the segment ("
            "found)");
  return NULL;
}

void remove_segment(uint32_t id, uint32_t pid, t_main_memory* main_memory,
                    t_log* logger)
{
  t_hole* new_hole = malloc(sizeof(t_hole));
  new_hole->base = 0;
  new_hole->size = 0;
  log_info(logger, "removing requested segment ID : %d, PID : %d", id, pid);
  t_segment* segment_aux =
      find_and_remove_segment(id, pid, main_memory, logger);
  pthread_mutex_lock(main_memory->main_memory_mutex);
  if (segment_aux == NULL)
  {
    log_error(logger, "Segment not found");
    pthread_mutex_unlock(main_memory->main_memory_mutex);
    free(new_hole);
    return;
  }

  bool has_hole_before = hole_before_segment(
      segment_aux->base, segment_aux->base + segment_aux->size,
      main_memory->holes);
  bool has_hole_after = hole_after_segment(
      segment_aux->base, segment_aux->base + segment_aux->size,
      main_memory->holes);

  if (has_hole_before && has_hole_after)
  {
    int indice1 = -1;
    int indice2 = -1;
    // SEGMENT BETWEEN HOLES
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* current_hole = list_iterator_next(iterator);
      if (current_hole->base + current_hole->size == segment_aux->base)
      {
        new_hole->base = current_hole->base;
        new_hole->size += current_hole->size + segment_aux->size;
        indice1 = i;
      }
      if (current_hole->base == segment_aux->base + segment_aux->size)
      {
        new_hole->size += current_hole->size;
        indice2 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    if (indice1 == -1 || indice2 == -1)
    {
      log_error(logger, "The holes adjacent to the segment were not found");
      pthread_mutex_unlock(main_memory->main_memory_mutex);
      free(segment_aux);
      free(new_hole);
      return;
    }
    if (indice1 < indice2)
    {
      list_remove_and_destroy_element(main_memory->holes, indice2, free);
      list_remove_and_destroy_element(main_memory->holes, indice1, free);
    }
    else
    {
      list_remove_and_destroy_element(main_memory->holes, indice1, free);
      list_remove_and_destroy_element(main_memory->holes, indice2, free);
    }
    list_add(main_memory->holes, new_hole);
    log_info(logger, "Segment in medio of holes");
  }
  else if (has_hole_before)
  {
    int indice1 = -1;
    // SEGMENT AFTER HOLE
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* current_hole = list_iterator_next(iterator);
      if (current_hole->base + current_hole->size == segment_aux->base)
      {
        new_hole->base = current_hole->base;
        new_hole->size = current_hole->size + segment_aux->size;
        indice1 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    list_remove_and_destroy_element(main_memory->holes, indice1, free);
    list_add(main_memory->holes, new_hole);
    log_info(logger, "Segment after hole");
  }
  else if (has_hole_after)
  {
    int indice2 = -1;
    // SEGMENT BEFORE HOLE
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* current_hole = list_iterator_next(iterator);
      if (current_hole->base == segment_aux->base + segment_aux->size)
      {
        new_hole->base = segment_aux->base;
        new_hole->size = current_hole->size + segment_aux->size;
        indice2 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    list_remove_and_destroy_element(main_memory->holes, indice2, free);
    list_add(main_memory->holes, new_hole);
    log_info(logger, "Segment before hole");
  }
  else
  {
    new_hole->base = segment_aux->base;
    new_hole->size = segment_aux->size;
    list_add(main_memory->holes, new_hole);
    log_info(logger, "Segment between two segments");
  }
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  free(segment_aux);
}

bool hole_before_segment(int base_segment, int final_segment, t_list* holes)
{
  t_list_iterator* iterator = list_iterator_create(holes);
  bool found = false;

  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);

    if ((current_hole->base + current_hole->size) == base_segment)
    {
      found = true;
      break;
    }
  }
  list_iterator_destroy(iterator);
  return found;
}
bool hole_after_segment(int base_segment, int final_segment, t_list* holes)
{
  t_list_iterator* iterator = list_iterator_create(holes);
  bool found = false;
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    if (current_hole->base == final_segment)
    {
      found = true;
      break;
    }
  }
  list_iterator_destroy(iterator);
  return found;
}

t_list* filter_process_segments(int pid, t_main_memory* main_memory,
                                t_log* logger)
{
  t_list* lista_filtrada = list_create();

  pthread_mutex_lock(main_memory->main_memory_mutex);
  for (int i = 0; i < list_size(main_memory->segments); i++)
  {
    // log_info(logger, "Filtrando segment");
    t_segment* current_segment = list_get(main_memory->segments, i);
    if (current_segment->pid == pid)
    {
      list_add(lista_filtrada, current_segment);
    }
  }
  pthread_mutex_unlock(main_memory->main_memory_mutex);

  return lista_filtrada;
}

void add_segments_to_packet(t_list* segments, t_packet* process_segment_table)
{
  t_list_iterator* iterator = list_iterator_create(segments);
  while (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    packet_append(process_segment_table, current_segment, sizeof(t_segment));
  }
  list_iterator_destroy(iterator);
}

t_segment* find_segment(t_main_memory* main_memory, uint32_t pid,
                        uint32_t segment_number)
{
  t_segment* found_seg = NULL;
  uint32_t contador_segments_pid = 0;

  pthread_mutex_lock(main_memory->main_memory_mutex);
  // scan the segments until the matching one is found correspondiente to the
  // pid y number of segment
  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  while (list_iterator_has_next(iterator))
  {
    t_segment* current_item = list_iterator_next(iterator);
    if (current_item->pid == pid)
    {
      if (contador_segments_pid == segment_number)
      {
        found_seg = current_item;
        break;
      }
      contador_segments_pid++;
    }
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  return found_seg;
}

int translate_logical_address(uint32_t pid, uint32_t logical_address,
                              uint32_t size, t_main_memory* main_memory,
                              t_log* logger)
{
  // Calculo of physical address
  int seg_max = main_memory->max_segment_size;
  uint32_t segment_number = logical_address / seg_max;
  uint32_t desplazamiento = logical_address % seg_max;

  t_segment* found_seg = find_segment(main_memory, pid, segment_number);
  if (found_seg == NULL)
  {
    log_error(logger, "Segment number %u was not found for process %u",
              segment_number, pid);
    return -1;
  }
  int physical_address = found_seg->base + desplazamiento;
  return physical_address;
}

int find_stick(int physical_address, t_list* connected_sticks,
               pthread_mutex_t* sticks_mutex, int* stick_offset)
{
  int base_acumulada = 0;
  int index = -1;
  int current_index = 0;

  pthread_mutex_lock(sticks_mutex);
  t_list_iterator* iterator = list_iterator_create(connected_sticks);
  while (list_iterator_has_next(iterator))
  {
    t_stick_data* current_item = list_iterator_next(iterator);
    if (physical_address >= base_acumulada &&
        physical_address < base_acumulada + current_item->stick_size)
    {
      // found the stick for the dir fisica
      *stick_offset = physical_address - base_acumulada;
      index = current_index;
      break;
    }
    current_index++;
    base_acumulada += current_item->stick_size;
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(sticks_mutex);
  return index;
}

char* read_from_sticks(int physical_address, int size, t_list* connected_sticks,
                       pthread_mutex_t* sticks_mutex, t_log* logger,
                       int socket_scheduler)
{
  char* result = malloc(size);
  int bytes_read = 0;
  int current_address = physical_address;
  log_info(logger, "Reading %d bytes from physical_address %d", size,
           physical_address);
  while (bytes_read < size)
  {
    int stick_offset = 0;
    int index = find_stick(current_address, connected_sticks, sticks_mutex,
                           &stick_offset);
    if (index == -1)
    {
      log_error(logger,
                "The computed physical address does not match any "
                "connected stick");
      free(result);
      return NULL;
    }
    pthread_mutex_lock(sticks_mutex);
    t_stick_data* stick = list_get(connected_sticks, index);
    int bytes_hasta_fin_stick = stick->stick_size - stick_offset;
    int bytes_to_read_count = size - bytes_read;
    // check whether the requested size fits in the stick or if it is split at
    // the medio
    if (bytes_to_read_count > bytes_hasta_fin_stick)
      bytes_to_read_count = bytes_hasta_fin_stick;
    // Send read request to the stick
    t_packet* packet = create_packet(OP_MEMORY_STICK_READ);
    packet_append(packet, &stick_offset, sizeof(int));
    packet_append(packet, &bytes_to_read_count, sizeof(int));
    if (!send_packet(packet, stick->socket_stick))
    {
      log_error(logger, "Error sending read packet to stick %d", index);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      free(result);
      pthread_mutex_unlock(sticks_mutex);
      return NULL;
    }
    destroy_packet(packet);
    pthread_mutex_unlock(sticks_mutex);

    // Recibo response
    int stick_socket =
        ((t_stick_data*)list_get(connected_sticks, index))->socket_stick;
    int op = receive_op_code(stick_socket);
    if (op == 0)
    {
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
    }
    if (op != OP_MEMORY_STICK_READ_DONE)
    {
      log_error(logger, "Opcode of response erroneo of the stick %d", index);
      free(result);
      return NULL;
    }
    // receive the bytes as a string
    int received_size = 0;
    char* fragment = receive_buffer(&received_size, stick_socket);

    memcpy(result + bytes_read, fragment, bytes_to_read_count);
    free(fragment);

    bytes_read += bytes_to_read_count;
    current_address += bytes_to_read_count;
  }

  log_info(logger, "Read %d bytes from physical_address %d", size,
           physical_address);
  return result;
}

int compute_process_size(t_process* process, t_main_memory* main_memory)
{
  int size = 0;
  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  pthread_mutex_lock(main_memory->main_memory_mutex);
  while (list_iterator_has_next(iterator))
  {
    t_segment* segment_aux = list_iterator_next(iterator);
    if (segment_aux->pid == process->pid)
    {
      size += segment_aux->size;
    }
  }
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  list_iterator_destroy(iterator);
  return size;
}

bool write_to_sticks(int pid, int physical_address, int bytes_to_read,
                     char* write_buffer, t_list* connected_sticks,
                     pthread_mutex_t* socket_list_mutex, t_log* logger,
                     int socket_scheduler)
{
  int stick_offset = 0;
  int index = find_stick(physical_address, connected_sticks, socket_list_mutex,
                         &stick_offset);

  pthread_mutex_lock(socket_list_mutex);

  int remaining = bytes_to_read;
  char* buffer_pointer = write_buffer;
  int current_offset = stick_offset;
  bool ok = true;

  for (int i = index; remaining > 0; i++)
  {
    t_stick_data* current_stick = list_get(connected_sticks, i);
    if (current_stick == NULL)
    {
      log_error(logger,
                "No more sticks available to finish the write "
                "(PID: %d, Phys. Addr: %d)",
                pid, physical_address);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      ok = false;
      break;
    }

    int available_space = current_stick->stick_size - current_offset;
    int to_write = remaining < available_space ? remaining : available_space;

    log_info(logger, "Starting to write to stick %d, offset %d", i,
             current_offset);

    t_packet* packet = create_packet(OP_MEMORY_STICK_WRITE);
    packet_append(packet, &current_offset, sizeof(int));
    packet_append(packet, buffer_pointer, to_write);
    packet_append(packet, &to_write, sizeof(int));

    if (!send_packet(packet, current_stick->socket_stick))
    {
      log_error(logger, "Error sending write packet to stick %d", i);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      destroy_packet(packet);
      ok = false;
      break;
    }
    destroy_packet(packet);

    log_info(logger, "##PID: %d - Write - Phys. Addr: %d - Size: %d", pid,
             physical_address, to_write);

    if (receive_op_code(current_stick->socket_stick) ==
        OP_MEMORY_STICK_WRITE_DONE)
    {
      char* buffer = receive_string(current_stick->socket_stick);
      free(buffer);
    }
    else
    {
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
    }

    buffer_pointer += to_write;
    remaining -= to_write;
    current_offset = 0; /* from the second stick onwards it always writes from
                          the start */
  }

  pthread_mutex_unlock(socket_list_mutex);
  return ok;
}