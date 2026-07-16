#include "kernel_scheduler/memory.h"

#include "utils/msg.h"

static bool respuesta_km_mem_alloc(t_colas* colas);
static bool hay_espacio(t_syscall_memory* mem_alloc, t_colas* colas);
static bool respuesta_km_mem_free(t_colas* colas);

bool allocate_memory(t_syscall_memory* mem_alloc, t_colas* colas)
{
  pthread_mutex_lock(&(colas->socket_km->mutex_socket));
  if (!hay_espacio(mem_alloc, colas))
  {
    pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
    return false;
  }

  int mem_alloc_size = sizeof(t_syscall_memory);
  bool comms = enviar_buffer(OP_SYSCALL_MEM_ALLOC, mem_alloc, mem_alloc_size,
                             colas->socket_km->socket_km);

  if (!comms)
  {
    logger_error(colas->logger,
                 "Error en la comunicacion con el Kernel Memory");
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
    return false;
  }

  comms = respuesta_km_mem_alloc(colas);
  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
  return comms;
}

bool free_memory(t_syscall_memory* mem_free, t_colas* colas)
{
  int mem_free_size = sizeof(t_syscall_memory);
  pthread_mutex_lock(&(colas->socket_km->mutex_socket));
  bool comms = enviar_buffer(OP_SYSCALL_MEM_FREE, mem_free, mem_free_size,
                             colas->socket_km->socket_km);

  if (!comms)
  {
    logger_error(colas->logger,
                 "Error en la comunicacion con el Kernel Memory");
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
    return false;
  }

  // Ahora aguardo a que el km me envíe el "OK"
  comms = respuesta_km_mem_free(colas);
  crear_hilo_rutina_des_suspension(colas);
  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
  return comms;
}

static bool respuesta_km_mem_alloc(t_colas* colas)
{
  int cod_op = -1;

  cod_op = recibir_operacion(colas->socket_km->socket_km);

  switch (cod_op)
  {
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      return true;
    case OP_TAMANIO_SEGMENTO_EXCEDIDO:
      free(recibir_string(colas->socket_km->socket_km));
      return false;
    case OP_MEMORIA_ALOJADA:
      free(recibir_string(colas->socket_km->socket_km));
      return true;
    case OP_COMPACTACION_NECESARIA:
      free(recibir_string(colas->socket_km->socket_km));
      rutina_compactacion(colas);
      return respuesta_km_mem_alloc(colas);
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return respuesta_km_mem_alloc(colas);
    default:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      return true;
  }
}

static bool hay_espacio(t_syscall_memory* mem_alloc, t_colas* colas)
{
  int espacio = espacio_disponible_sin_mutex(colas, mem_alloc->pid);
  return espacio >= mem_alloc->tamanio;
}

static bool respuesta_km_mem_free(t_colas* colas)
{
  int cod_op = -1;

  cod_op = recibir_operacion(colas->socket_km->socket_km);

  switch (cod_op)
  {
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      return false;
    case OP_MEMORIA_LIBERADA:
      free(recibir_string(colas->socket_km->socket_km));
      return true;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return respuesta_km_mem_free(colas);
    default:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      return false;
  }
}
