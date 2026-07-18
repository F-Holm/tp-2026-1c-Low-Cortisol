#include "kernel_memory/protocolo.h"

void aniadir_lista_mtx(t_list* lista, pthread_mutex_t* mutex, void* elemento)
{
  pthread_mutex_lock(mutex);
  list_add(lista, elemento);
  pthread_mutex_unlock(mutex);
}

bool recibir_id_cpu(t_datos_cpu* datos_cpu)
{
  if (recibir_operacion(datos_cpu->socket_cpu) == OP_ID_CPU)
  {
    char* id_cpu = recibir_string(datos_cpu->socket_cpu);
    logger_info(datos_cpu->logger, "## CPU %s Conectada", id_cpu);
    datos_cpu->id = atoi(id_cpu);
    free(id_cpu);
    return true;
  }
  else
  {
    logger_info(datos_cpu->logger,
                "No se pudo realizar la conexion con el CPU ya que no se "
                "envio la operacion de ID");
    terminar_comunicacion(datos_cpu->socket_cpu);
    return false;
  }
  return false;
}

bool recibir_tamanio_stick(t_datos_stick* datos_stick)
{
  if (recibir_operacion(datos_stick->socket_stick) == OP_TAMANIO_MEMORIA)
  {
    char* tamanio = recibir_string(datos_stick->socket_stick);
    logger_info(datos_stick->logger, "## Memory Stick de %s bytes Conectada",
                tamanio);
    datos_stick->tamanio_stick = atoi(tamanio);
    free(tamanio);
    return true;
  }
  else
  {
    logger_info(datos_stick->logger,
                "No se pudo realizar la conexion con la stick ya que no se "
                "envio la operacion de tamaño");
    terminar_comunicacion(datos_stick->socket_stick);
    return false;
  }
  return false;
}

bool recibir_puerto_escucha_stick(t_datos_stick* datos_stick)
{
  if (recibir_operacion(datos_stick->socket_stick) == OP_PUERTO)
  {
    char* puerto = recibir_string(datos_stick->socket_stick);
    logger_info(datos_stick->logger, "Puerto de Memory Stick recibido %s",
                puerto);
    datos_stick->puerto_stick = atoi(puerto);
    free(puerto);
    return true;
  }
  else
  {
    logger_info(datos_stick->logger,
                "No se pudo realizar la conexion con la stick ya que no se "
                "envio la operacion puerto");
    terminar_comunicacion(datos_stick->socket_stick);
    return false;
  }
  return false;
}

void agregar_conexion_stick(t_datos_kernel_mem* datos_kernel_memory,
                            t_datos_stick* datos_stick)
{
  aniadir_lista_mtx(datos_kernel_memory->sticks_conectados,
                    datos_kernel_memory->mutex_lista_sockets, datos_stick);
  return;
}

void agregar_conexion_cpu(t_datos_kernel_mem* datos_kernel_memory,
                          t_datos_cpu* datos_cpu)
{
  aniadir_lista_mtx(datos_kernel_memory->cpus_conectados,
                    datos_kernel_memory->mutex_lista_sockets, datos_cpu);
  return;
}

void enviar_sticks_conectadas(t_list* sticks_conectados,
                              pthread_mutex_t* mutex_lista_sockets,
                              t_datos_cpu* datos_cpu)
{
  pthread_mutex_lock(mutex_lista_sockets);
  int total_sticks = list_size(sticks_conectados);

  for (int i = 0; i < total_sticks; i++)
  {
    t_paquete* paquete = crear_paquete(OP_PAQUETE);
    t_datos_stick* stick_actual =
        (t_datos_stick*)list_get(sticks_conectados, i);

    char puerto[6];
    snprintf(puerto, sizeof(puerto), "%u", stick_actual->puerto_stick);

    agregar_string_a_paquete(paquete, stick_actual->ip_memory_stick);
    agregar_string_a_paquete(paquete, puerto);
    agregar_a_paquete(paquete, &stick_actual->tamanio_stick, sizeof(int));

    enviar_paquete(paquete, datos_cpu->socket_cpu);
    eliminar_paquete(paquete);
  }
  pthread_mutex_unlock(mutex_lista_sockets);
}

void enviar_conexion_cpu(t_datos_stick* datos_stick, t_list* cpus_conectados)
{
  if (list_is_empty(cpus_conectados))
  {
    return;
  }
  t_paquete* paquete = crear_paquete(OP_PAQUETE);

  agregar_string_a_paquete(paquete, datos_stick->ip_memory_stick);

  char puerto[6];
  snprintf(puerto, sizeof(puerto), "%u", datos_stick->puerto_stick);
  agregar_string_a_paquete(paquete, puerto);
  agregar_a_paquete(paquete, &datos_stick->tamanio_stick, sizeof(int));

  for (int i = 0; i < list_size(cpus_conectados); i++)
  {
    t_datos_cpu* cpu_actual = (t_datos_cpu*)list_get(cpus_conectados, i);
    enviar_paquete(paquete, cpu_actual->socket_cpu);
  }
  eliminar_paquete(paquete);
}

int calcular_memoria_total(t_list* sticks_conectados,
                           pthread_mutex_t* mutex_lista_sockets)
{
  int total = 0;
  for (int i = 0; i < list_size(sticks_conectados); i++)
  {
    pthread_mutex_lock(mutex_lista_sockets);
    t_datos_stick* stick_actual =
        (t_datos_stick*)list_get(sticks_conectados, i);
    pthread_mutex_unlock(mutex_lista_sockets);
    total += stick_actual->tamanio_stick;
  }
  return total;
}

int calcular_espacio_libre(t_list* huecos, pthread_mutex_t* mutex_huecos,
                           t_logger* logger)
{
  int total = 0;
  logger_info(logger, "Calculando huecos...");
  t_list_iterator* iterador = list_iterator_create(huecos);
  while (list_iterator_has_next(iterador))
  {
    t_hueco* hueco_actual = list_iterator_next(iterador);
    total += hueco_actual->size;
  }
  list_iterator_destroy(iterador);
  logger_info(logger, "hay %d espacio libre", total);
  return total;
}

t_proceso* buscar_proceso(t_list* lista_procesos,
                          pthread_mutex_t* mutex_procesos, uint32_t pid)
{
  pthread_mutex_lock(mutex_procesos);
  for (int i = 0; i < list_size(lista_procesos); i++)
  {
    t_proceso* proceso = list_get(lista_procesos, i);
    if (proceso->pid == pid)
    {
      pthread_mutex_unlock(mutex_procesos);
      return proceso;
    }
  }
  pthread_mutex_unlock(mutex_procesos);
  return NULL;
}

t_memoria_principal* aniadir_memoria_total(
    t_memoria_principal* memoria_principal, int memoria_total)
{
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  int base_nueva = memoria_principal->tamanio_total;
  memoria_principal->tamanio_total += memoria_total;

  t_hueco* hueco_contiguo = NULL;
  t_list_iterator* iterador = list_iterator_create(memoria_principal->huecos);
  while (list_iterator_has_next(iterador))
  {
    t_hueco* h = list_iterator_next(iterador);
    if (h->base + h->size == base_nueva)
    {
      hueco_contiguo = h;
      break;
    }
  }
  list_iterator_destroy(iterador);

  if (hueco_contiguo != NULL)
  {
    hueco_contiguo->size += memoria_total;
  }
  else
  {
    t_hueco* hueco_nuevo = malloc(sizeof(t_hueco));
    hueco_nuevo->base = base_nueva;
    hueco_nuevo->size = memoria_total;
    list_add(memoria_principal->huecos, hueco_nuevo);
  }

  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  return memoria_principal;
}

static t_hueco algoritmo_seleccionador(
    uint32_t tamanio, t_list* huecos_actuales, t_logger* logger,
    t_allocation_strategy allocation_strategy)
{
  t_hueco hueco_elegido = {-1, -1};
  t_list_iterator* iterador = list_iterator_create(huecos_actuales);
  while (list_iterator_has_next(iterador))
  {
    t_hueco* hueco_actual = list_iterator_next(iterador);
    if (hueco_actual->size >= tamanio)
    {
      switch (allocation_strategy)
      {
        case BEST:
          if (hueco_elegido.size == -1 ||
              hueco_elegido.size > hueco_actual->size)
            hueco_elegido = *hueco_actual;
          break;
        case WORST:
          if (hueco_elegido.size == -1 ||
              hueco_elegido.size < hueco_actual->size)
            hueco_elegido = *hueco_actual;
          break;
      }
    }
  }
  list_iterator_destroy(iterador);
  if (hueco_elegido.size == -1)
  {
    logger_info(logger, "No hay huecos disponibles");
  }
  return hueco_elegido;
}

static void actualizar_tabla_huecos(t_memoria_principal* memoria_principal,
                                    t_hueco hueco_elegido, uint32_t tamanio)
{
  t_list_iterator* iterador = list_iterator_create(memoria_principal->huecos);
  while (list_iterator_has_next(iterador))
  {
    t_hueco* hueco_actual = list_iterator_next(iterador);
    if (hueco_actual->base == hueco_elegido.base)
    {
      hueco_actual->size -= tamanio;
      hueco_actual->base += tamanio;
      if (hueco_actual->size == 0)
      {
        list_iterator_remove(iterador);
        free(hueco_actual);
      }
      break;
    }
  }
  list_iterator_destroy(iterador);
}

static t_hueco selector_de_huecos(uint32_t tamanio, t_logger* logger,
                                  t_memoria_principal* memoria)
{
  t_hueco hueco_elegido = {-1, -1};
  if (memoria->allocation_strategy == BEST)
    hueco_elegido =
        algoritmo_seleccionador(tamanio, memoria->huecos, logger, BEST);
  else if (memoria->allocation_strategy == WORST)
    hueco_elegido =
        algoritmo_seleccionador(tamanio, memoria->huecos, logger, WORST);
  else
  {
    logger_error(logger,
                 "La opción de selección de huecos elegida no es válida.");
    return hueco_elegido;
  }
  actualizar_tabla_huecos(memoria, hueco_elegido, tamanio);
  return hueco_elegido;
}

static void actualizar_lista_segmentos(t_memoria_principal* memoria_principal,
                                       t_hueco hueco_elegido, int tamanio,
                                       uint32_t pid, uint32_t id)
{
  t_segmento* segmento = malloc(sizeof(t_segmento));
  segmento->base = hueco_elegido.base;
  segmento->pid = pid;
  segmento->id = id;
  segmento->size = tamanio;
  list_add(memoria_principal->segmentos, segmento);
}

void crear_segmento(uint32_t id, uint32_t pid, int size,
                    t_memoria_principal* memoria_principal,
                    int socket_scheduler, t_logger* logger)
{
  // Chequeo de segmentation fault
  if (size > memoria_principal->tamanio_maximo_segmento)
    enviar_string(OP_TAMANIO_SEGMENTO_EXCEDIDO,
                  "El tamaño solicitado supera el tamaño máximo de segmento.",
                  socket_scheduler);

  // Chequeo de cantidad de memoria disponible
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  if (calcular_espacio_libre(memoria_principal->huecos,
                             memoria_principal->mutex_memoria_principal,
                             logger) < size)
  {
    logger_info(logger, "No hay espacio suficiente para crear el segmento");
    enviar_string(OP_MEMORIA_INSUFICIENTE, "No hay memoria suficiente",
                  socket_scheduler);
    pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  }
  else
  {
    logger_info(logger, "Creando segmento con id %u, pid %u y tamaño %d", id,
                pid, size);
    t_hueco hueco_elegido = selector_de_huecos(size, logger, memoria_principal);

    // Chequeo de compactación
    if (hueco_elegido.size == -1)
    {
      logger_info(logger, "Es necesario compactar la memoria");
      notificar_compactacion(socket_scheduler);
      compactar_memoria(socket_scheduler, memoria_principal);
      selector_de_huecos(size, logger, memoria_principal);
    }
    actualizar_lista_segmentos(memoria_principal, hueco_elegido, size, pid, id);
    pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
    enviar_string(OP_MEMORIA_ALOJADA, "Se ha alojado la memoria",
                  socket_scheduler);
    logger_info(logger, "## PID: %u - Segmento Creado %u - Tamaño: %d", pid, id,
                size);
  }
}

bool compactar_memoria(int socket_scheduler,
                       t_memoria_principal* memoria_principal)
{
  compactar_segmentos(memoria_principal->segmentos);
  memoria_principal->huecos = compactar_huecos(
      memoria_principal->tamanio_total,
      calcular_base_final_segmento(memoria_principal->segmentos));
  usleep(memoria_principal->compaction_delay * 1000);
  enviar_string(OP_COMPACTACION_FINALIZADA, "Se finalizo la compactacion",
                socket_scheduler);
  return true;
}

void compactar_segmentos(t_list* segmentos)
{
  for (int i = 0; i < list_size(segmentos) - 1; i++)
  {
    t_segmento* segmento_actual = list_get(segmentos, i);
    t_segmento* siguiente_segmento = list_get(segmentos, i + 1);
    if (i == 0)
    {
      segmento_actual->base = 0;
    }
    siguiente_segmento->base = segmento_actual->base + segmento_actual->size;
  }
}

int calcular_base_final_segmento(t_list* segmentos)
{
  t_segmento* ultimo_segmento = list_get(segmentos, list_size(segmentos) - 1);
  return ultimo_segmento->base + ultimo_segmento->size;
}

t_list* compactar_huecos(int memoria_total, int base_final_segmento)
{
  t_list* huecos = list_create();
  t_hueco* hueco_final = malloc(sizeof(t_hueco));
  hueco_final->base = base_final_segmento;
  hueco_final->size = memoria_total - base_final_segmento;
  list_add(huecos, hueco_final);
  return huecos;
}

void notificar_compactacion(int socket_scheduler)
{
  enviar_string(OP_COMPACTACION_NECESARIA, "Es necesario compactar la memoria",
                socket_scheduler);
  int operacion = recibir_operacion(socket_scheduler);
  if (operacion == OP_PUEDE_COMPACTAR)
  {
    char* mensaje = recibir_string(socket_scheduler);
    free(mensaje);
  }
}

t_segmento* buecar_y_eliminar_segmento(uint32_t id, uint32_t pid,
                                       t_memoria_principal* memoria_principal,
                                       t_logger* logger)
{
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);

  logger_info(logger, "recorriendo lista de segmentos de tamanio %d:",
              list_size(memoria_principal->segmentos));
  for (int i = 0; i < list_size(memoria_principal->segmentos); i++)
  {
    t_segmento* segmento_actual = list_get(memoria_principal->segmentos, i);
    if (segmento_actual->id == id && segmento_actual->pid == pid)
    {
      t_segmento* segmento = list_remove(memoria_principal->segmentos, i);
      logger_info(logger, "se ha eliminado el segmento con ID: %d, PID: %d",
                  segmento->id, segmento->pid);
      pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
      return segmento;
    }
  }
  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  logger_error(logger, "ha ocurrido un error con la eliminacion del segmento");
  return NULL;
}

void eliminar_segmento(uint32_t id, uint32_t pid,
                       t_memoria_principal* memoria_principal, t_logger* logger)
{
  t_hueco* nuevo_hueco = malloc(sizeof(t_hueco));
  nuevo_hueco->base = 0;
  nuevo_hueco->size = 0;
  logger_info(logger, "eliminando segmento requerido ID : %d, PID : %d", id,
              pid);
  t_segmento* segmento_aux =
      buecar_y_eliminar_segmento(id, pid, memoria_principal, logger);
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  if (segmento_aux == NULL)
  {
    logger_error(logger, "No se encontro segmento");
    pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
    return;
  }
  if (hueco_antes_segmento(segmento_aux->base,
                           segmento_aux->base + segmento_aux->size,
                           memoria_principal->huecos) &&
      hueco_despues_segmento(segmento_aux->base,
                             segmento_aux->base + segmento_aux->size,
                             memoria_principal->huecos))
  {
    int indice1 = -1;
    int indice2 = -1;
    // SEGMENTO EN MEDIO DE HUECOS
    for (int i = 0; i < list_size(memoria_principal->huecos); i++)
    {
      t_hueco* hueco_actual = list_get(memoria_principal->huecos, i);
      if (hueco_actual->base + hueco_actual->size == segmento_aux->base)
      {
        hueco_actual->base = hueco_actual->base;
        hueco_actual->size = hueco_actual->size + segmento_aux->size;
        indice1 = i;
      }
      if (hueco_actual->base == segmento_aux->base + segmento_aux->size)
      {
        hueco_actual->size += hueco_actual->size;
        indice2 = i;
      }
    }
    if (indice1 == -1 || indice2 == -1)
    {
      logger_error(logger,
                   "No se encontraron los huecos adyacentes al segmento");
      pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
      free(segmento_aux);
      free(nuevo_hueco);
      return;
    }
    list_remove_and_destroy_element(memoria_principal->huecos, indice1, free);
    list_remove_and_destroy_element(memoria_principal->huecos, indice2, free);
    list_add(memoria_principal->huecos, nuevo_hueco);
    logger_info(logger, "Segmento en medio de huecos");
  }
  else if (hueco_antes_segmento(segmento_aux->base,
                                segmento_aux->base + segmento_aux->size,
                                memoria_principal->huecos))
  {
    int indice1 = -1;
    // SEGMENTO DESPUES DE HUECO
    for (int i = 0; i < list_size(memoria_principal->huecos); i++)
    {
      t_hueco* hueco_actual = list_get(memoria_principal->huecos, i);
      if (hueco_actual->base + hueco_actual->size == segmento_aux->base)
      {
        hueco_actual->base = hueco_actual->base;
        hueco_actual->size = hueco_actual->size + segmento_aux->size;
        indice1 = i;
      }
    }
    list_remove_and_destroy_element(memoria_principal->huecos, indice1, free);
    list_add(memoria_principal->huecos, nuevo_hueco);
    logger_info(logger, "Segmento despues de hueco");
  }
  else if (hueco_despues_segmento(segmento_aux->base,
                                  segmento_aux->base + segmento_aux->size,
                                  memoria_principal->huecos))
  {
    int indice2 = -1;
    // SEGMENTO ANTES DE HUECO
    for (int i = 0; i < list_size(memoria_principal->huecos); i++)
    {
      t_hueco* hueco_actual = list_get(memoria_principal->huecos, i);
      if (hueco_actual->base == segmento_aux->base + segmento_aux->size)
      {
        hueco_actual->size += hueco_actual->size;
        indice2 = i;
      }
    }
    list_remove_and_destroy_element(memoria_principal->huecos, indice2, free);
    list_add(memoria_principal->huecos, nuevo_hueco);
    logger_info(logger, "Segmento antes de hueco");
  }
  else
  {
    nuevo_hueco->base = segmento_aux->base;
    nuevo_hueco->size = segmento_aux->size;
    list_add(memoria_principal->huecos, nuevo_hueco);
    logger_info(logger, "Segmento entre dos segmentos");
  }
  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  free(segmento_aux);
}

bool hueco_antes_segmento(int base_segmento, int final_segmento, t_list* huecos)
{
  for (int i = 0; i < list_size(huecos); i++)
  {
    t_hueco* hueco_actual = list_get(huecos, i);
    if ((hueco_actual->base + hueco_actual->size) == base_segmento)
    {
      return true;
    }
  }
  return false;
}

bool hueco_despues_segmento(int base_segmento, int final_segmento,
                            t_list* huecos)
{
  for (int i = 0; i < list_size(huecos); i++)
  {
    t_hueco* hueco_actual = list_get(huecos, i);
    if (hueco_actual->base == final_segmento)
    {
      return true;
    }
  }
  return false;
}

t_list* filtrar_segmentos_proceso(int pid,
                                  t_memoria_principal* memoria_principal,
                                  t_logger* logger)
{
  t_list* lista_filtrada = list_create();

  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  for (int i = 0; i < list_size(memoria_principal->segmentos); i++)
  {
    logger_info(logger, "Filtrando segmento");
    t_segmento* segmento_actual = list_get(memoria_principal->segmentos, i);
    if (segmento_actual->pid == pid)
    {
      list_add(lista_filtrada, segmento_actual);
    }
  }
  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);

  return lista_filtrada;
}

void agregar_segmentos_a_paquete(t_list* segmentos,
                                 t_paquete* tabla_segmentos_proceso)
{
  for (int i = 0; i < list_size(segmentos); i++)
  {
    t_segmento* segmento_actual = list_get(segmentos, i);
    agregar_a_paquete(tabla_segmentos_proceso, segmento_actual,
                      sizeof(t_segmento));
  }
}

t_segmento* buscar_segmento(t_memoria_principal* memoria_principal,
                            uint32_t pid, uint32_t num_segmento)
{
  t_segmento* seg_encontrado = NULL;
  uint32_t contador_segmentos_pid = 0;

  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  // recorro los segmentos hasta encontrar el correspondiente al pid y numero de
  // segmento
  t_list_iterator* iterador =
      list_iterator_create(memoria_principal->segmentos);
  while (list_iterator_has_next(iterador))
  {
    t_segmento* item_actual = list_iterator_next(iterador);
    if (item_actual->pid == pid)
    {
      if (contador_segmentos_pid == num_segmento)
      {
        seg_encontrado = item_actual;
        break;
      }
      contador_segmentos_pid++;
    }
  }
  list_iterator_destroy(iterador);
  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  return seg_encontrado;
}

int traducir_direccion_logica(uint32_t pid, uint32_t direccion_logica,
                              uint32_t tamanio,
                              t_memoria_principal* memoria_principal,
                              t_logger* logger)
{
  // Calculo de direccion fisica
  int seg_max = memoria_principal->tamanio_maximo_segmento;
  uint32_t num_segmento = direccion_logica / seg_max;
  uint32_t desplazamiento = direccion_logica % seg_max;

  t_segmento* seg_encontrado =
      buscar_segmento(memoria_principal, pid, num_segmento);
  if (seg_encontrado == NULL)
  {
    logger_error(logger,
                 "No se encontro el numero de segmento %u para el proceso %u",
                 num_segmento, pid);
    return -1;
  }
  int dir_fisica = seg_encontrado->base + desplazamiento;
  return dir_fisica;
}

int encontrar_stick(int direccion_fisica, t_list* sticks_conectados,
                    pthread_mutex_t* mutex_sticks, int* offset_en_stick)
{
  int base_acumulada = 0;
  int indice = -1;
  int indice_actual = 0;

  pthread_mutex_lock(mutex_sticks);
  t_list_iterator* iterador = list_iterator_create(sticks_conectados);
  while (list_iterator_has_next(iterador))
  {
    t_datos_stick* item_actual = list_iterator_next(iterador);
    if (direccion_fisica >= base_acumulada &&
        direccion_fisica < base_acumulada + item_actual->tamanio_stick)
    {
      // encontre el stick de la dir fisica
      *offset_en_stick = direccion_fisica - base_acumulada;
      indice = indice_actual;
      break;
    }
    indice_actual++;
    base_acumulada += item_actual->tamanio_stick;
  }
  list_iterator_destroy(iterador);
  pthread_mutex_unlock(mutex_sticks);
  return indice;
}

char* leer_de_sticks(int direccion_fisica, int tamanio,
                     t_list* sticks_conectados, pthread_mutex_t* mutex_sticks,
                     t_logger* logger, int socket)
{
  char* resultado = malloc(tamanio + 1);
  resultado[tamanio] = '\0';
  int bytes_leidos = 0;
  int dir_actual = direccion_fisica;
  logger_info(logger, "Leyendo %d bytes desde dir_fisica %d", tamanio,
              direccion_fisica);
  while (bytes_leidos < tamanio)
  {
    int offset_en_stick = 0;
    int indice = encontrar_stick(dir_actual, sticks_conectados, mutex_sticks,
                                 &offset_en_stick);
    if (indice == -1)
    {
      logger_error(logger,
                   "La direccion fisica calculada no corresponde a ningún "
                   "stick conectado");
      free(resultado);
      return NULL;
    }
    pthread_mutex_lock(mutex_sticks);
    t_datos_stick* stick = list_get(sticks_conectados, indice);
    int bytes_hasta_fin_stick = stick->tamanio_stick - offset_en_stick;
    int cant_bytes_a_leer = tamanio - bytes_leidos;
    // reviso si el tamaño pedido entra en el stick o si esta cortado al medio
    if (cant_bytes_a_leer > bytes_hasta_fin_stick)
      cant_bytes_a_leer = bytes_hasta_fin_stick;
    // Envio pedido de lectura al stick
    t_paquete* paquete = crear_paquete(OP_MEMORY_STICK_LEER);
    agregar_a_paquete(paquete, &offset_en_stick, sizeof(int));
    agregar_a_paquete(paquete, &cant_bytes_a_leer, sizeof(int));
    if (!enviar_paquete(paquete, stick->socket_stick))
    {
      logger_error(logger, "Error al enviar paquete de lectura al stick %d",
                   indice);
      enviar_string(OP_MEMORIA_CORRUPTA, "Stick no disponible", socket);
      free(resultado);
      pthread_mutex_unlock(mutex_sticks);
      return NULL;
    }
    eliminar_paquete(paquete);
    pthread_mutex_unlock(mutex_sticks);

    // Recibo respuesta
    int stick_socket =
        ((t_datos_stick*)list_get(sticks_conectados, indice))->socket_stick;
    int op = recibir_operacion(stick_socket);
    if (op != OP_MEMORY_STICK_LEIDO)
    {
      logger_error(logger, "Opcode de respuesta erroneo del stick %d", indice);
      free(resultado);
      return NULL;
    }
    // Recibo los bytes como string
    int size_recibido = 0;
    char* fragmento = recibir_buffer(&size_recibido, stick_socket);

    memcpy(resultado + bytes_leidos, fragmento, cant_bytes_a_leer);
    free(fragmento);

    bytes_leidos += cant_bytes_a_leer;
    dir_actual += cant_bytes_a_leer;
  }

  logger_info(logger, "Lectura de %d bytes desde dir_fisica %d", tamanio,
              direccion_fisica);
  return resultado;
}

char* cortar_cadena(int longitud_corte, char* cadena)
{
  if (longitud_corte <= 0)
  {
    char* vacia = malloc(1);
    vacia[0] = '\0';
    return vacia;
  }

  int longitud = strlen(cadena);

  if (longitud_corte > longitud)
    longitud_corte = longitud;

  char* nueva = malloc(longitud_corte + 1);

  memcpy(nueva, cadena, longitud_corte);
  nueva[longitud_corte] = '\0';

  return nueva;
}

int calcular_tamanio_proceso(t_proceso* proceso)
{
  int tamanio = 0;
  for (int i = 0; i < list_size(proceso->segmentos); i++)
  {
    t_segmento* segmento_aux = list_get(proceso->segmentos, i);
    tamanio += segmento_aux->size;
  }
  return tamanio;
}

bool escribir_en_sticks(int pid, int dir_fisica, int tamanio_a_leer,
                        char* string_escribir, t_list* sticks_conectados,
                        pthread_mutex_t* mutex_lista_sockets, t_logger* logger,
                        int socket_scheduler)
{
  int offset_en_stick = 0;
  int indice = encontrar_stick(dir_fisica, sticks_conectados,
                               mutex_lista_sockets, &offset_en_stick);
  pthread_mutex_lock(mutex_lista_sockets);
  logger_info(logger, "Empiezo a escribir en el stick %d, offset %d", indice,
              offset_en_stick);
  t_datos_stick* stick_a_escribir_inicial = list_get(sticks_conectados, indice);
  int tamanio = stick_a_escribir_inicial->tamanio_stick - offset_en_stick -
                tamanio_a_leer;
  t_paquete* paquete = crear_paquete(OP_MEMORY_STICK_ESCRIBIR);
  if (tamanio < 0)
  {
    int tamanio_sumado =
        stick_a_escribir_inicial->tamanio_stick - offset_en_stick;
    char* cadena_cortada = cortar_cadena(tamanio_sumado, string_escribir);
    logger_info(logger,
                "se tubo que cortar la cadena a : %s para poder escribir en el "
                "stick %d",
                cadena_cortada, indice);
    int tamanio_cortado = strlen(cadena_cortada);
    agregar_a_paquete(paquete, &offset_en_stick, sizeof(int));
    agregar_string_a_paquete(paquete, cadena_cortada);
    agregar_a_paquete(paquete, &tamanio_cortado, sizeof(int));
    if (!enviar_paquete(paquete, stick_a_escribir_inicial->socket_stick))
    {
      logger_error(logger, "Error al enviar paquete de lectura al stick %d",
                   indice);
      enviar_string(OP_MEMORIA_CORRUPTA, "Stick no disponible",
                    socket_scheduler);
      free(paquete);
      pthread_mutex_unlock(mutex_lista_sockets);
      return false;
    }
    eliminar_paquete(paquete);
    logger_info(logger, "##PID: %d - Escritura - Dir. Fisica: %d - Tamaño: %d",
                pid, dir_fisica, tamanio_cortado);
    if (recibir_operacion(stick_a_escribir_inicial->socket_stick) ==
        OP_MEMORY_STICK_ESCRITO)
    {
      char* buffer = recibir_string(stick_a_escribir_inicial->socket_stick);
      free(buffer);
    }

    for (int i = indice + 1; tamanio_sumado < tamanio_a_leer; i++)
    {
      t_datos_stick* stick_a_escribir = list_get(sticks_conectados, i);
      tamanio_sumado += stick_a_escribir->tamanio_stick;
      t_paquete* paquete2 = crear_paquete(OP_MEMORY_STICK_ESCRIBIR);
      int cero = 0;
      agregar_a_paquete(paquete2, &cero, sizeof(int));
      char* cadena_cortada = cortar_cadena(stick_a_escribir->tamanio_stick,
                                           string_escribir + tamanio_sumado);
      int tamanio_cortado2 = strlen(cadena_cortada);
      agregar_string_a_paquete(paquete2, cadena_cortada);
      agregar_a_paquete(paquete2, &tamanio_cortado2, sizeof(int));
      if (!enviar_paquete(paquete2, stick_a_escribir->socket_stick))
      {
        logger_error(logger, "Error al enviar paquete de lectura al stick %d",
                     i);
        enviar_string(OP_MEMORIA_CORRUPTA, "Stick no disponible",
                      socket_scheduler);
        free(cadena_cortada);
        free(paquete2);
        pthread_mutex_unlock(mutex_lista_sockets);
        return false;
      }
      free(cadena_cortada);
      eliminar_paquete(paquete2);
      logger_info(logger,
                  "##PID: %d - Escritura - Dir. Fisica: %d - Tamaño: %d", pid,
                  0, tamanio_cortado2);
      if (recibir_operacion(stick_a_escribir->socket_stick) ==
          OP_MEMORY_STICK_ESCRITO)
      {
        char* buffer = recibir_string(stick_a_escribir->socket_stick);
        free(buffer);
      }
    }
    free(cadena_cortada);
  }
  else
  {
    int tamanio_string = strlen(string_escribir);
    agregar_a_paquete(paquete, &offset_en_stick, sizeof(int));
    agregar_string_a_paquete(paquete, string_escribir);
    agregar_a_paquete(paquete, &tamanio_string, sizeof(int));
    if (!enviar_paquete(paquete, stick_a_escribir_inicial->socket_stick))
    {
      logger_error(logger, "Error al enviar paquete de lectura al stick %d",
                   indice);
      enviar_string(OP_MEMORIA_CORRUPTA, "Stick no disponible",
                    socket_scheduler);
      free(paquete);
      pthread_mutex_unlock(mutex_lista_sockets);
      return false;
    }
    eliminar_paquete(paquete);
    logger_info(logger, "##PID: %d - Escritura - Dir. Fisica: %d - Tamaño: %d",
                pid, dir_fisica, tamanio_string);
    if (recibir_operacion(stick_a_escribir_inicial->socket_stick) ==
        OP_MEMORY_STICK_ESCRITO)
    {
      char* buffer = recibir_string(stick_a_escribir_inicial->socket_stick);
      free(buffer);
    }
  }
  pthread_mutex_unlock(mutex_lista_sockets);
  return true;
}