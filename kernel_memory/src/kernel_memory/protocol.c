#include "kernel_memory/protocol.h"

void list_add_mtx(t_list* lista, pthread_mutex_t* mutex, void* elemento)
{
  pthread_mutex_lock(mutex);
  list_add(lista, elemento);
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
    log_info(stick_data->logger, "## Memory Stick de %s bytes Conectada", size);
    stick_data->stick_size = atoi(size);
    free(size);
    return true;
  }
  else
  {
    log_info(stick_data->logger,
             "Could not connect to the stick because it could not "
             "sent the size operationño");
    close_communication(stick_data->socket_stick);
    return false;
  }
  return false;
}

bool receive_stick_listen_port(t_stick_data* stick_data)
{
  if (receive_op_code(stick_data->socket_stick) == OP_PORT)
  {
    char* puerto = receive_string(stick_data->socket_stick);
    log_info(stick_data->logger, "Puerto de Memory Stick recibido %s", puerto);
    stick_data->stick_port = atoi(puerto);
    free(puerto);
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
    t_stick_data* stick_actual = (t_stick_data*)list_get(connected_sticks, i);

    char puerto[6];
    snprintf(puerto, sizeof(puerto), "%u", stick_actual->stick_port);

    packet_append_string(packet, stick_actual->ip_memory_stick);
    packet_append_string(packet, puerto);
    packet_append(packet, &stick_actual->stick_size, sizeof(int));

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

  char puerto[6];
  snprintf(puerto, sizeof(puerto), "%u", stick_data->stick_port);
  packet_append_string(packet, puerto);
  packet_append(packet, &stick_data->stick_size, sizeof(int));

  for (int i = 0; i < list_size(connected_cpus); i++)
  {
    t_cpu_data* cpu_actual = (t_cpu_data*)list_get(connected_cpus, i);
    send_packet(packet, cpu_actual->socket_cpu);
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
    t_stick_data* stick_actual = (t_stick_data*)list_get(connected_sticks, i);
    pthread_mutex_unlock(socket_list_mutex);
    total += stick_actual->stick_size;
  }
  return total;
}

int compute_free_space(t_list* holes, pthread_mutex_t* holes_mutex,
                       t_log* logger)
{
  int total = 0;
  log_info(logger, "Calculando holes...");
  t_list_iterator* iterator = list_iterator_create(holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* hole_actual = list_iterator_next(iterator);
    total += hole_actual->size;
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
  int base_nueva = main_memory->total_size;
  main_memory->total_size += memory_total;

  t_hole* hole_contiguo = NULL;
  t_list_iterator* iterator = list_iterator_create(main_memory->holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* h = list_iterator_next(iterator);
    if (h->base + h->size == base_nueva)
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
    t_hole* hole_nuevo = malloc(sizeof(t_hole));
    hole_nuevo->base = base_nueva;
    hole_nuevo->size = memory_total;
    list_add(main_memory->holes, hole_nuevo);
  }

  pthread_mutex_unlock(main_memory->main_memory_mutex);
  return main_memory;
}

static t_hole hole_selection_algorithm(
    uint32_t size, t_list* holes_actuales, t_log* logger,
    t_allocation_strategy allocation_strategy)
{
  t_hole chosen_hole = {-1, -1};
  t_list_iterator* iterator = list_iterator_create(holes_actuales);
  while (list_iterator_has_next(iterator))
  {
    t_hole* hole_actual = list_iterator_next(iterator);
    if (hole_actual->size >= size)
    {
      switch (allocation_strategy)
      {
        case BEST:
          if (chosen_hole.size == -1 || chosen_hole.size > hole_actual->size)
            chosen_hole = *hole_actual;
          break;
        case WORST:
          if (chosen_hole.size == -1 || chosen_hole.size < hole_actual->size)
            chosen_hole = *hole_actual;
          break;
      }
    }
  }
  list_iterator_destroy(iterator);
  if (chosen_hole.size == -1)
  {
    log_info(logger, "No hay holes disponibles");
  }
  return chosen_hole;
}

static void update_hole_table(t_main_memory* main_memory, t_hole chosen_hole,
                              uint32_t size)
{
  t_list_iterator* iterator = list_iterator_create(main_memory->holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* hole_actual = list_iterator_next(iterator);
    if (hole_actual->base == chosen_hole.base)
    {
      hole_actual->size -= size;
      hole_actual->base += size;
      if (hole_actual->size == 0)
      {
        list_iterator_remove(iterator);
        free(hole_actual);
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
              "The chosen hole-selection option is not validón de selección de "
              "holes elegida no es válida.");
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
  // Chequeo de segmentation fault
  if (size > main_memory->max_segment_size)
    send_string(OP_SEGMENT_SIZE_EXCEEDED,
                "The requested size exceeds the max segment sizeño solicitado "
                "supera el sizeño máximo de segment.",
                socket_scheduler);

  // Chequeo de count de memory disponible
  pthread_mutex_lock(main_memory->main_memory_mutex);
  if (compute_free_space(main_memory->holes, main_memory->main_memory_mutex,
                         logger) < size)
  {
    log_info(logger, "Not enough space to create the segment");
    send_string(OP_NOT_ENOUGH_MEMORY, "No hay memory suficiente",
                socket_scheduler);
    pthread_mutex_unlock(main_memory->main_memory_mutex);
  }
  else
  {
    log_info(logger, "Creating segment with id %u, pid %u and sizeño %d", id,
             pid, size);
    t_hole chosen_hole = select_hole(size, logger, main_memory);

    // Chequeo de compactación
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
    log_info(logger, "## PID: %u - Segment created %u - Tamaño: %d", pid, id,
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
      t_segment* siguiente_segment = list_iterator_next(iterator);

      siguiente_segment->base = current_segment->base + current_segment->size;

      current_segment = siguiente_segment;
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
  int operacion = receive_op_code(socket_scheduler);
  if (operacion == OP_CAN_COMPACT)
  {
    char* mensaje = receive_string(socket_scheduler);
    free(mensaje);
  }
}

t_segment* find_and_remove_segment(uint32_t id, uint32_t pid,
                                   t_main_memory* main_memory, t_log* logger)
{
  pthread_mutex_lock(main_memory->main_memory_mutex);

  log_info(logger, "recorriendo lista de segments de size %d:",
           list_size(main_memory->segments));

  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  t_segment* segment_encontrado = NULL;

  while (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    if (current_segment->id == id && current_segment->pid == pid)
    {
      list_iterator_remove(iterator);
      segment_encontrado = current_segment;
      log_info(logger, "removed the segment with ID: %d, PID: %d",
               segment_encontrado->id, segment_encontrado->pid);
      break;
    }
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  if (segment_encontrado != NULL)
  {
    return segment_encontrado;
  }
  log_error(
      logger,
      "ha ocurrido un error con la eliminacion del segment (no encontrado)");
  return NULL;
}

void remove_segment(uint32_t id, uint32_t pid, t_main_memory* main_memory,
                    t_log* logger)
{
  t_hole* nuevo_hole = malloc(sizeof(t_hole));
  nuevo_hole->base = 0;
  nuevo_hole->size = 0;
  log_info(logger, "eliminando segment requerido ID : %d, PID : %d", id, pid);
  t_segment* segment_aux =
      find_and_remove_segment(id, pid, main_memory, logger);
  pthread_mutex_lock(main_memory->main_memory_mutex);
  if (segment_aux == NULL)
  {
    log_error(logger, "Segment not found");
    pthread_mutex_unlock(main_memory->main_memory_mutex);
    free(nuevo_hole);
    return;
  }

  bool hay_antes = hole_before_segment(segment_aux->base,
                                       segment_aux->base + segment_aux->size,
                                       main_memory->holes);
  bool hay_despues = hole_after_segment(segment_aux->base,
                                        segment_aux->base + segment_aux->size,
                                        main_memory->holes);

  if (hay_antes && hay_despues)
  {
    int indice1 = -1;
    int indice2 = -1;
    // SEGMENTO EN MEDIO DE HUECOS
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* hole_actual = list_iterator_next(iterator);
      if (hole_actual->base + hole_actual->size == segment_aux->base)
      {
        nuevo_hole->base = hole_actual->base;
        nuevo_hole->size += hole_actual->size + segment_aux->size;
        indice1 = i;
      }
      if (hole_actual->base == segment_aux->base + segment_aux->size)
      {
        nuevo_hole->size += hole_actual->size;
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
      free(nuevo_hole);
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
    list_add(main_memory->holes, nuevo_hole);
    log_info(logger, "Segment en medio de holes");
  }
  else if (hay_antes)
  {
    int indice1 = -1;
    // SEGMENTO DESPUES DE HUECO
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* hole_actual = list_iterator_next(iterator);
      if (hole_actual->base + hole_actual->size == segment_aux->base)
      {
        nuevo_hole->base = hole_actual->base;
        nuevo_hole->size = hole_actual->size + segment_aux->size;
        indice1 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    list_remove_and_destroy_element(main_memory->holes, indice1, free);
    list_add(main_memory->holes, nuevo_hole);
    log_info(logger, "Segment despues de hole");
  }
  else if (hay_despues)
  {
    int indice2 = -1;
    // SEGMENTO ANTES DE HUECO
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* hole_actual = list_iterator_next(iterator);
      if (hole_actual->base == segment_aux->base + segment_aux->size)
      {
        nuevo_hole->base = segment_aux->base;
        nuevo_hole->size = hole_actual->size + segment_aux->size;
        indice2 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    list_remove_and_destroy_element(main_memory->holes, indice2, free);
    list_add(main_memory->holes, nuevo_hole);
    log_info(logger, "Segment antes de hole");
  }
  else
  {
    nuevo_hole->base = segment_aux->base;
    nuevo_hole->size = segment_aux->size;
    list_add(main_memory->holes, nuevo_hole);
    log_info(logger, "Segment entre dos segments");
  }
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  free(segment_aux);
}

bool hole_before_segment(int base_segment, int final_segment, t_list* holes)
{
  t_list_iterator* iterator = list_iterator_create(holes);
  bool encontrado = false;

  while (list_iterator_has_next(iterator))
  {
    t_hole* hole_actual = list_iterator_next(iterator);

    if ((hole_actual->base + hole_actual->size) == base_segment)
    {
      encontrado = true;
      break;
    }
  }
  list_iterator_destroy(iterator);
  return encontrado;
}
bool hole_after_segment(int base_segment, int final_segment, t_list* holes)
{
  t_list_iterator* iterator = list_iterator_create(holes);
  bool encontrado = false;
  while (list_iterator_has_next(iterator))
  {
    t_hole* hole_actual = list_iterator_next(iterator);
    if (hole_actual->base == final_segment)
    {
      encontrado = true;
      break;
    }
  }
  list_iterator_destroy(iterator);
  return encontrado;
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
  t_segment* seg_encontrado = NULL;
  uint32_t contador_segments_pid = 0;

  pthread_mutex_lock(main_memory->main_memory_mutex);
  // scan the segments until the matching one is found correspondiente al pid y
  // number de segment
  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  while (list_iterator_has_next(iterator))
  {
    t_segment* item_actual = list_iterator_next(iterator);
    if (item_actual->pid == pid)
    {
      if (contador_segments_pid == segment_number)
      {
        seg_encontrado = item_actual;
        break;
      }
      contador_segments_pid++;
    }
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  return seg_encontrado;
}

int translate_logical_address(uint32_t pid, uint32_t logical_address,
                              uint32_t size, t_main_memory* main_memory,
                              t_log* logger)
{
  // Calculo de direccion fisica
  int seg_max = main_memory->max_segment_size;
  uint32_t segment_number = logical_address / seg_max;
  uint32_t desplazamiento = logical_address % seg_max;

  t_segment* seg_encontrado = find_segment(main_memory, pid, segment_number);
  if (seg_encontrado == NULL)
  {
    log_error(logger, "Segment number %u was not found for process %u",
              segment_number, pid);
    return -1;
  }
  int physical_address = seg_encontrado->base + desplazamiento;
  return physical_address;
}

int find_stick(int physical_address, t_list* connected_sticks,
               pthread_mutex_t* sticks_mutex, int* stick_offset)
{
  int base_acumulada = 0;
  int indice = -1;
  int indice_actual = 0;

  pthread_mutex_lock(sticks_mutex);
  t_list_iterator* iterator = list_iterator_create(connected_sticks);
  while (list_iterator_has_next(iterator))
  {
    t_stick_data* item_actual = list_iterator_next(iterator);
    if (physical_address >= base_acumulada &&
        physical_address < base_acumulada + item_actual->stick_size)
    {
      // found the stick for the dir fisica
      *stick_offset = physical_address - base_acumulada;
      indice = indice_actual;
      break;
    }
    indice_actual++;
    base_acumulada += item_actual->stick_size;
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(sticks_mutex);
  return indice;
}

char* read_from_sticks(int physical_address, int size, t_list* connected_sticks,
                       pthread_mutex_t* sticks_mutex, t_log* logger,
                       int socket_scheduler)
{
  char* resultado = malloc(size);
  int bytes_leidos = 0;
  int dir_actual = physical_address;
  log_info(logger, "Leyendo %d bytes desde physical_address %d", size,
           physical_address);
  while (bytes_leidos < size)
  {
    int stick_offset = 0;
    int indice =
        find_stick(dir_actual, connected_sticks, sticks_mutex, &stick_offset);
    if (indice == -1)
    {
      log_error(logger,
                "La direccion fisica calculada no corresponde a ningún "
                "stick conectado");
      free(resultado);
      return NULL;
    }
    pthread_mutex_lock(sticks_mutex);
    t_stick_data* stick = list_get(connected_sticks, indice);
    int bytes_hasta_fin_stick = stick->stick_size - stick_offset;
    int cant_bytes_a_leer = size - bytes_leidos;
    // check whether the requested size fits in the stick o si esta cortado al
    // medio
    if (cant_bytes_a_leer > bytes_hasta_fin_stick)
      cant_bytes_a_leer = bytes_hasta_fin_stick;
    // Envio pedido de lectura al stick
    t_packet* packet = create_packet(OP_MEMORY_STICK_READ);
    packet_append(packet, &stick_offset, sizeof(int));
    packet_append(packet, &cant_bytes_a_leer, sizeof(int));
    if (!send_packet(packet, stick->socket_stick))
    {
      log_error(logger, "Error sending read packet to stick %d", indice);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      free(resultado);
      pthread_mutex_unlock(sticks_mutex);
      return NULL;
    }
    destroy_packet(packet);
    pthread_mutex_unlock(sticks_mutex);

    // Recibo respuesta
    int stick_socket =
        ((t_stick_data*)list_get(connected_sticks, indice))->socket_stick;
    int op = receive_op_code(stick_socket);
    if (op == 0)
    {
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
    }
    if (op != OP_MEMORY_STICK_READ_DONE)
    {
      log_error(logger, "Opcode de respuesta erroneo del stick %d", indice);
      free(resultado);
      return NULL;
    }
    // receive the bytes como string
    int size_recibido = 0;
    char* fragmento = receive_buffer(&size_recibido, stick_socket);

    memcpy(resultado + bytes_leidos, fragmento, cant_bytes_a_leer);
    free(fragmento);

    bytes_leidos += cant_bytes_a_leer;
    dir_actual += cant_bytes_a_leer;
  }

  log_info(logger, "Lectura de %d bytes desde physical_address %d", size,
           physical_address);
  return resultado;
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
  int indice = find_stick(physical_address, connected_sticks, socket_list_mutex,
                          &stick_offset);

  pthread_mutex_lock(socket_list_mutex);

  int restante = bytes_to_read;
  char* puntero_buffer = write_buffer;
  int offset_actual = stick_offset;
  bool ok = true;

  for (int i = indice; restante > 0; i++)
  {
    t_stick_data* stick_actual = list_get(connected_sticks, i);
    if (stick_actual == NULL)
    {
      log_error(logger,
                "No more sticks available to finish the write "
                "(PID: %d, Phys. Addr: %d)",
                pid, physical_address);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      ok = false;
      break;
    }

    int espacio_disponible = stick_actual->stick_size - offset_actual;
    int a_escribir =
        restante < espacio_disponible ? restante : espacio_disponible;

    log_info(logger, "Starting to write to stick %d, offset %d", i,
             offset_actual);

    t_packet* packet = create_packet(OP_MEMORY_STICK_WRITE);
    packet_append(packet, &offset_actual, sizeof(int));
    packet_append(packet, puntero_buffer, a_escribir);
    packet_append(packet, &a_escribir, sizeof(int));

    if (!send_packet(packet, stick_actual->socket_stick))
    {
      log_error(logger, "Error sending write packet to stick %d", i);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      destroy_packet(packet);
      ok = false;
      break;
    }
    destroy_packet(packet);

    log_info(logger, "##PID: %d - Write - Phys. Addr: %d - Tamaño: %d", pid,
             physical_address, a_escribir);

    if (receive_op_code(stick_actual->socket_stick) ==
        OP_MEMORY_STICK_WRITE_DONE)
    {
      char* buffer = receive_string(stick_actual->socket_stick);
      free(buffer);
    }
    else
    {
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
    }

    puntero_buffer += a_escribir;
    restante -= a_escribir;
    offset_actual =
        0; /* a partir del segundo stick siempre se escribe desde el inicio */
  }

  pthread_mutex_unlock(socket_list_mutex);
  return ok;
}