#include "kernel_scheduler/io.h"

#include <string.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/io.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/msg.h"
#include "utils/registros.h"

int obtener_tipo_io(int socket_fd, t_log* logger)
{
  if (recibir_operacion(socket_fd) != OP_TIPO_IO)
  {
    log_error(logger, "## Error en el tipo de operación. Expected: OP_TIPO_IO");
    return -1;
  }

  char* buffer = recibir_string(socket_fd);
  int tipo_io;

  if (strcmp(buffer, V_TIPO_IO[E_STDIN]) == 0)
    tipo_io = E_STDIN;
  else if (strcmp(buffer, V_TIPO_IO[E_STDOUT]) == 0)
    tipo_io = E_STDOUT;
  else if (strcmp(buffer, V_TIPO_IO[E_SLEEP]) == 0)
    tipo_io = E_SLEEP;
  else
  {
    log_error(logger, "## Tipo de IO no válido: %s", buffer);
    free(buffer);
    return -1;
  }

  log_info(logger, "## IO de tipo %s conectada", buffer);
  free(buffer);
  return tipo_io;
}

bool atender_nuevo_io(int sockets_io[3], int socket_fd, t_log* logger)
{
  if (!responder_handshake(socket_fd, MID_KERNEL_SCHEDULER, logger))
    return false;

  int tipo_io = obtener_tipo_io(socket_fd, logger);
  if (tipo_io == -1)
    return false;

  if (sockets_io[tipo_io] != -1)
  {
    log_error(logger, "## IO de tipo repetido: %d. Cerrando conexión", tipo_io);
    close(socket_fd);
    return false;
  }

  sockets_io[tipo_io] = socket_fd;
  return true;
}

void cerrar_io(int sockets_io[3])
{
  for (int i = 0; i < 3; i++)
  {
    if (sockets_io[i] > 0)
      close(sockets_io[i]);
  }
}

void retirar_elem_cola(t_cola_mutex_io* cola_mutex, t_log* logger,
                       t_pcb** pcb_post_io)
{
  pthread_mutex_lock(&cola_mutex->mutex_sockets_io);
  log_info(logger, "## (%d) Toma el Mutex de la Cola de IO",
           (*pcb_post_io)->pid);
  if (queue_is_empty(cola_mutex->cola_mutex))
  {
    log_warning(logger, "## No hay procesos bloqueados esperando IO");
    pthread_mutex_unlock(&cola_mutex->mutex_sockets_io);
    return;
  }
  *pcb_post_io = queue_pop(cola_mutex->cola_mutex);
  pthread_mutex_unlock(&cola_mutex->mutex_sockets_io);
  log_info(logger, "## (%d) Libera el Mutex de la Cola de IO",
           (*pcb_post_io)->pid);
}

void reingresar_proceso(t_pcb** pcb_post_io, t_log* logger,
                        t_cola_ready* cola_ready)
{
  pthread_mutex_lock(cola_ready->mutex_cola);
  log_info(logger, "## (%d) Toma el Mutex de la Lista de Procesos",
           (*pcb_post_io)->pid);

  log_info(logger, "## (%d) Pasa de BLOCK a READY", (*pcb_post_io)->pid);

  queue_push(cola_ready->cola, *pcb_post_io);
  log_info(logger, "## (%d) finalizó IO y pasa a READY / SUSP. READY",
           (*pcb_post_io)->pid);

  pthread_mutex_unlock(cola_ready->mutex_cola);
  log_info(logger, "## (%d) Libera el Mutex de la Lista de Procesos",
           (*pcb_post_io)->pid);
}



//FALTA CAMBIAR LA FUNCION ADAPTADA AL STRUCT t_io
bool io_stdin(int sockets_io[], t_cola* cola_block, t_cola_ready* cola_ready,
              t_peticion_stdin* peticion, t_kernel_scheduler_recursos* recursos)
{
  // Envio peticion a IO
  int peticion_size = sizeof(t_peticion_stdin);
  send(sockets_io[E_STDIN], &peticion_size, sizeof(int), 0);
  send(sockets_io[E_STDIN], peticion, peticion_size, 0);

  // recibo
  int tamanio;
  peticion->buffer = recibir_buffer(&tamanio, sockets_io[E_STDIN]);
  if (peticion->buffer == NULL)
  {
    log_error(recursos->logger, "## Error al recibir la respuesa de IO");
    return false;
  }
  // Le envio el paquete al Kernel Memory para que escriba en la memoria
  t_paquete* paquete = crear_paquete();
  paquete->codigo_operacion = OP_ESCRIBIR_EN_MEMORIA;
  agregar_a_paquete(paquete, &peticion->pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, &peticion->direccion_logica, sizeof(uint32_t));
  agregar_a_paquete(paquete, &peticion->tamanio_a_leer, sizeof(uint32_t));
  agregar_a_paquete(paquete, peticion->buffer, peticion->tamanio_a_leer);
  log_info(recursos->logger, "## (%d) - Solicitó syscall: STDIN - Tamaño: %d",
           peticion->pid, peticion->tamanio_a_leer);
  bool envio_correcto = enviar_paquete(paquete, recursos->socket_kernel_memory);
  if (!envio_correcto)
  {
    log_error(recursos->logger,
              "## Error al enviar la respuesta de IO a Kernel Memory");
    eliminar_paquete(paquete);
    return false;
  }
  eliminar_paquete(paquete);
  free(peticion->buffer);

  // Saco el proceso de la cola de mutex
  t_pcb* pcb_post_io = NULL;
  retirar_elem_cola(cola_block->cola, recursos->logger, &pcb_post_io);
  if (pcb_post_io == NULL)
  {
    return false;
  }
  // Lo paso a READY
  reingresar_proceso(&pcb_post_io, t_log * logger, cola_ready);

  return true;
}
