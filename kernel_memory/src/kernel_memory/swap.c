#include "kernel_memory/swap.h"

// SUSPENDER PROCESO

static int seleccionar_bloque_libre(t_datos_swap* datos_swap)
{
  for (int i = 0; i < datos_swap->tamanio_swap / datos_swap->tamanio_bloque;
       i++)
  {
    t_datos_bloque* bloque = list_get(datos_swap->lista_bloques, i);
    if (bloque->pid == -1)
      return i;
  }
  logger_info(datos_swap->logger, "No hay bloques libres en swap");
  return -1;
}

static int agregar_bloque_lista_swap(t_segmento* segmento, int contador,
                                     t_datos_swap* datos_swap, t_logger* logger)
/*retorna el numero de bloque agregado o -1 si no se pudo agregar*/
{
  int num_bloque_libre = seleccionar_bloque_libre(datos_swap);
  if (num_bloque_libre != -1)
  {
    t_datos_bloque* bloque_libre =
        list_get(datos_swap->lista_bloques, num_bloque_libre);
    bloque_libre->num_segmento = segmento->id;
    bloque_libre->num_bloque_del_segmento = contador;
    bloque_libre->pid = segmento->pid;
    bloque_libre->tamanio_segmento = segmento->size;
    logger_info(
        logger,
        "Agregando bloque a swap: PID %d, Segmento %d, Bloque del segmento %d.",
        bloque_libre->pid, bloque_libre->num_segmento,
        bloque_libre->num_bloque_del_segmento);
  }
  else
  {
    logger_info(
        logger,
        "No se puede agregar el segmento a swap: No hay bloques libres.");
  }
  return num_bloque_libre;
}

static void escribir_bloque_en_swap(int num_bloque, char* contenido,
                                    int cantidad_bytes, t_datos_swap* swap)
{
  t_paquete* paquete = crear_paquete(OP_ESCRIBIR_DISCO);
  agregar_a_paquete(paquete, &num_bloque, sizeof(int));
  char* buffer_auxiliar = calloc(swap->tamanio_bloque, 1);
  memcpy(buffer_auxiliar, contenido, cantidad_bytes);
  agregar_a_paquete(paquete, buffer_auxiliar, swap->tamanio_bloque);
  enviar_paquete(paquete, swap->socket_swap);
  eliminar_paquete(paquete);
  free(buffer_auxiliar);
}

void eliminar_segmentos_del_proceso(t_list* segmentos_a_eliminar, uint32_t pid,
                                    t_datos_scheduler* datos_scheduler)
{
  t_list_iterator* it = list_iterator_create(segmentos_a_eliminar);
  while (list_iterator_has_next(it))
  {
    t_segmento* seg = list_iterator_next(it);
    eliminar_segmento(seg->id, pid, datos_scheduler->memoria_principal,
                      datos_scheduler->logger);
  }
  list_iterator_destroy(it);
  list_destroy(segmentos_a_eliminar);
}

void suspender_proceso(t_proceso* proceso_a_suspender,
                       t_datos_scheduler* datos_scheduler)
{
  if (proceso_a_suspender == NULL)
  {
    logger_error(datos_scheduler->logger,
                 "suspender_proceso recibió un proceso NULL");
    return;
  }

  int tamanio_bloque = datos_scheduler->datos_swap->tamanio_bloque;
  bool proceso_suspendido = false;
  t_list* segmentos_a_eliminar = list_create();
  t_list_iterator* iterador =
      list_iterator_create(datos_scheduler->memoria_principal->segmentos);
  while (list_iterator_has_next(iterador))
  {
    t_segmento* segmento_actual = list_iterator_next(iterador);
    if (segmento_actual->pid == proceso_a_suspender->pid)
    {
      int cant_bloques_x_segmento =
          (segmento_actual->size + tamanio_bloque - 1) / tamanio_bloque;
      bool segmento_suspendido = false;
      for (int i = 0; i < cant_bloques_x_segmento; i++)
      {
        int offset = i * tamanio_bloque;
        int cantidad_bytes_a_leer =
            (segmento_actual->size - offset) < tamanio_bloque
                ? (segmento_actual->size - offset)
                : tamanio_bloque;
        char* contenido = leer_de_sticks(
            segmento_actual->base + offset, cantidad_bytes_a_leer,
            datos_scheduler->sticks_conectados,
            datos_scheduler->mutex_lista_sockets, datos_scheduler->logger,
            datos_scheduler->socket_scheduler);
        int num_bloque = agregar_bloque_lista_swap(segmento_actual, i,
                                                   datos_scheduler->datos_swap,
                                                   datos_scheduler->logger);
        if (num_bloque != -1)
        {
          escribir_bloque_en_swap(num_bloque, contenido, cantidad_bytes_a_leer,
                                  datos_scheduler->datos_swap);
          segmento_suspendido = true;
        }
        else
        {
          logger_info(datos_scheduler->logger,
                      "No se pudo suspender el proceso PID %d: No hay bloques "
                      "libres en swap.",
                      proceso_a_suspender->pid);
          enviar_string(
              OP_SUSPENSION_NO_EXITOSA,
              "No se pudo suspender el proceso porque swap esta lleno.",
              datos_scheduler->socket_scheduler);
          segmento_suspendido = false;
          free(contenido);
          break;
        }
        free(contenido);
      }
      if (segmento_suspendido)
      {
        list_add(segmentos_a_eliminar, segmento_actual);
        proceso_suspendido = true;
      }
      else
      {
        list_iterator_destroy(iterador);
        return;
      }
    }
  }
  list_iterator_destroy(iterador);
  eliminar_segmentos_del_proceso(segmentos_a_eliminar, proceso_a_suspender->pid,
                                 datos_scheduler);
  if (proceso_suspendido)
  {
    logger_info(datos_scheduler->logger,
                "Se suspendio correctamente el proceso PID %d.",
                proceso_a_suspender->pid);
    enviar_string(OP_SUSPENSION_EXITOSA, "", datos_scheduler->socket_scheduler);
  }
  else
  {
    logger_info(datos_scheduler->logger, "No se encontro el proceso PID %d.",
                proceso_a_suspender->pid);
    enviar_string(
        OP_SUSPENSION_NO_EXITOSA,
        "No se pudo suspender el proceso porque no se encontro su pid.",
        datos_scheduler->socket_scheduler);
  }
}

// DES-SUSPENDER PROCESO

static bool puede_des_suspender(uint32_t pid,
                                t_datos_scheduler* datos_scheduler)
{
  int tamanio_proceso = 0;
  t_list* bloques = datos_scheduler->datos_swap->lista_bloques;
  for (int i = 0; i < list_size(bloques); i++)
  {
    t_datos_bloque* bloque = list_get(bloques, i);
    if (bloque->pid == pid && bloque->num_bloque_del_segmento == 0)
    {
      tamanio_proceso += bloque->tamanio_segmento;
    }
  }
  pthread_mutex_lock(
      datos_scheduler->memoria_principal->mutex_memoria_principal);
  int espacio_libre = calcular_espacio_libre(
      datos_scheduler->memoria_principal->huecos,
      datos_scheduler->memoria_principal->mutex_memoria_principal,
      datos_scheduler->logger);
  pthread_mutex_unlock(
      datos_scheduler->memoria_principal->mutex_memoria_principal);
  logger_info(
      datos_scheduler->logger,
      "PID %u: tamanio requerido para des-suspender %d, espacio libre %d", pid,
      tamanio_proceso, espacio_libre);
  return tamanio_proceso <= espacio_libre;
}

static bool regenerar_segmento(uint32_t id, uint32_t pid, int size,
                               t_memoria_principal* memoria_principal,
                               int socket_scheduler, t_logger* logger)
{
  // Chequeo de cantidad de memoria disponible
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  if (calcular_espacio_libre(memoria_principal->huecos,
                             memoria_principal->mutex_memoria_principal,
                             logger) < size)
  {
    logger_info(logger, "No hay espacio suficiente para regenerar el segmento");
    enviar_string(OP_DES_SUSPENSION_NO_EXITOSA, "No hay memoria suficiente",
                  socket_scheduler);
    pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
    return false;
  }
  else
  {
    logger_info(logger, "Regenerando segmento con id %u, pid %u y tamaño %d",
                id, pid, size);
    t_hueco hueco_elegido = selector_de_huecos(size, logger, memoria_principal);

    // Chequeo de compactación
    if (hueco_elegido.size == -1)
    {
      logger_error(logger, "## No se pudo asignar ningun hueco.");
      enviar_string(OP_DES_SUSPENSION_NO_EXITOSA,
                    "No se pudieron asignar huecos.", socket_scheduler);
      return false;
    }
    actualizar_lista_segmentos(memoria_principal, hueco_elegido, size, pid, id);
    pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
    logger_info(logger, "## PID: %u - Segmento Regenerado %u - Tamaño: %d", pid,
                id, size);
    return true;
  }
}

static char* leer_bloque_en_swap(int num_bloque, t_datos_swap* swap)
{
  enviar_buffer(OP_LEER_DISCO, &num_bloque, sizeof(int), swap->socket_swap);
  int a;
  char* contenido_leido = (char*)recibir_buffer(&a, swap->socket_swap);
  //Agrega el \0 por si el bloque estaba lleno y no tenia 0 al final
  char* contenido_terminado = malloc(a + 1);
  memcpy(contenido_terminado, contenido_leido, a);
  contenido_terminado[a] = '\0';
  free(contenido_leido);
  return contenido_terminado;
}

static int quitar_bloque_lista_swap(int num_bloque, t_datos_swap* datos_swap,
                                    t_logger* logger)
/*retorna el numero de bloque eliminado o -1 si no se pudo eliminar*/
{
  t_datos_bloque* bloque_a_eliminar =
      list_get(datos_swap->lista_bloques, num_bloque);
  if (bloque_a_eliminar == NULL)
  {
    logger_info(logger,
                "No se pudo eliminar el bloque de swap numero: %d porque no "
                "existe tal bloque.",
                num_bloque);
    return -1;
  }
  bloque_a_eliminar->num_segmento = -1;
  bloque_a_eliminar->num_bloque_del_segmento = -1;
  bloque_a_eliminar->pid = -1;
  bloque_a_eliminar->tamanio_segmento = -1;
  logger_info(logger, "Eliminando el bloque de swap numero: %d.", num_bloque);
  return num_bloque;
}

static int calcular_direccion_con_offset(int tamanio_bloque,
                                         t_memoria_principal* memoria_principal,
                                         uint32_t pid, t_datos_bloque* bloque)
{
  t_segmento* segmento =
      buscar_segmento(memoria_principal, pid, bloque->num_segmento);
  return (segmento->base + tamanio_bloque * bloque->num_bloque_del_segmento);
}

void des_suspender_proceso(uint32_t pid, t_datos_scheduler* datos_scheduler)
{
  int tamanio_bloque = datos_scheduler->datos_swap->tamanio_bloque;
  bool proceso_encontrado = false;
  if (!puede_des_suspender(pid, datos_scheduler))
  {
    logger_info(datos_scheduler->logger,
                "No se pudo suspender porque el proceso no cabe en la memoria");
    enviar_string(OP_DES_SUSPENSION_NO_EXITOSA, "El proceso no cabe en memoria",
                  datos_scheduler->socket_scheduler);
    return;
  }
  t_list_iterator* iterador =
      list_iterator_create(datos_scheduler->datos_swap->lista_bloques);
  while (list_iterator_has_next(iterador))
  {
    t_datos_bloque* bloque = list_iterator_next(iterador);
    if (bloque->pid == pid)
    {
      /* funciona porque se asume que los bloques de un mismo segmento estan en
       * orden por la logica de la funcion seleccionar_bloque_libre */
      proceso_encontrado = true;
      if (bloque->num_bloque_del_segmento == 0)
      {
        bool regeneracion_exitosa = regenerar_segmento(
            bloque->num_segmento, pid, bloque->tamanio_segmento,
            datos_scheduler->memoria_principal,
            datos_scheduler->socket_scheduler, datos_scheduler->logger);
        if (!regeneracion_exitosa)
        {
          list_iterator_destroy(iterador);
          return;
        }
      }
      char* contenido =
          leer_bloque_en_swap(bloque->num_bloque, datos_scheduler->datos_swap);
      int direccion_a_escribir = calcular_direccion_con_offset(
          tamanio_bloque, datos_scheduler->memoria_principal, pid, bloque);
      escribir_en_sticks(pid, direccion_a_escribir, tamanio_bloque, contenido,
                         datos_scheduler->sticks_conectados,
                         datos_scheduler->mutex_lista_sockets,
                         datos_scheduler->logger,
                         datos_scheduler->socket_scheduler);
      free(contenido);
      if (bloque->num_bloque !=
          quitar_bloque_lista_swap(bloque->num_bloque,
                                   datos_scheduler->datos_swap,
                                   datos_scheduler->logger))
      {
        logger_info(datos_scheduler->logger,
                    "Como no se encontro el bloque nro: %d en swap, no se "
                    "puede des-suspender al PID %d.",
                    bloque->num_bloque, pid);
        enviar_string(OP_DES_SUSPENSION_NO_EXITOSA,
                      "No se pudo quitar el bloque de swap.",
                      datos_scheduler->socket_scheduler);
        list_iterator_destroy(iterador);
        return;
      }
    }
  }
  list_iterator_destroy(iterador);
  if (!proceso_encontrado)
  {
    logger_info(datos_scheduler->logger,
                "El proceso no esta suspendido o no se encontraron sus bloques "
                "en disco.");
    enviar_string(OP_DES_SUSPENSION_NO_EXITOSA,
                  "No se des-suspendio el proceso porque no se encontraron sus "
                  "bloques en disco.",
                  datos_scheduler->socket_scheduler);
  }
  else
  {
    logger_info(datos_scheduler->logger,
                "Se des-suspendio correctamente el proceso PID %d.", pid);
    enviar_string(OP_DES_SUSPENSION_EXITOSA, "",
                  datos_scheduler->socket_scheduler);
  }
}