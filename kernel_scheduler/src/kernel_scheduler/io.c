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

bool retirar_lista_io(t_pcb* pcb, t_list* cola_io, t_logger* logger)
{
  if (list_remove_element(cola_io, pcb) == 0)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al retirar el proceso de la lista de IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## PID %d - Retirado de la lista de IO", pcb->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));
  return true;
}

// FALTA CAMBIAR LA FUNCION ADAPTADA AL STRUCT t_io
bool io_stdin(t_pcb* pcb, t_io* io_stdin, t_cola* block, t_cola_ready* ready,
              t_lista* susp_block, t_lista* susp_ready,
              t_peticion_stdin* peticion, t_logger* logger,
              t_socket_kernel_memory socket_km)
{
  // Envio peticion a IO
  int peticion_size = sizeof(t_peticion_stdin);
  pthread_mutex_lock(&(io->mutex_socket));
  send(io->socket, &peticion_size, sizeof(int), 0);
  send(io->socket, peticion, peticion_size, 0);
  pthread_mutex_unlock(&(io->mutex_socket));

  // recibo
  int tamanio;
  pthread_mutex_lock(&(logger->mutex_logger));
  peticion->buffer = recibir_buffer(&tamanio, io->socket);
  pthread_mutex_unlock(&(logger->mutex_logger));
  if (peticion->buffer == NULL)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## Error al recibir la respuesa de IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }
  // Le envio el paquete al Kernel Memory para que escriba en la memoria
  t_paquete* paquete = crear_paquete();
  paquete->codigo_operacion = OP_ESCRIBIR_EN_MEMORIA;
  agregar_a_paquete(paquete, &peticion->pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, &peticion->direccion_logica, sizeof(uint32_t));
  agregar_a_paquete(paquete, &peticion->tamanio_a_leer, sizeof(uint32_t));
  agregar_a_paquete(paquete, peticion->buffer, peticion->tamanio_a_leer);

  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## (%d) - Solicitó syscall: STDIN - Tamaño: %d",
           peticion->pid, peticion->tamanio_a_leer);
  pthread_mutex_unlock(&(logger->mutex_logger));
  pthread_mutex_lock(&(socket_km->mutex_socket));
  bool envio_correcto = enviar_paquete(paquete, socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  if (!envio_correcto)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al enviar la respuesta de IO a Kernel Memory");
    pthread_mutex_unlock(&(logger->mutex_logger));
    eliminar_paquete(paquete);
    return false;
  }
  eliminar_paquete(paquete);
  free(peticion->buffer);

  if (!retirar_lista_io(pcb, io_stdin->cola_io, logger))
  {
    return false;
  }
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  if (pcb->tiempo_bloqueado == 0)
  {
    cambio_susp_block_susp(pcb, susp_block, susp_ready, logger);
  }
  else
  {
    cambio_block_ready(pcb, block, ready, logger);
  }

  return true;
}
