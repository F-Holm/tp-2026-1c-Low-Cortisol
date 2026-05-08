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


bool io_stdin(t_pcb* pcb, t_io* io_stdin, t_cola* block, t_cola_ready* ready,
              t_lista* susp_block, t_lista* susp_ready,
              t_peticion_stdin* peticion, t_logger* logger,
              t_socket_kernel_memory* socket_km)
{
  // Envio peticion a IO
  int peticion_size = sizeof(t_peticion_stdin);
  pthread_mutex_lock(&(io->mutex_io));
  enviar_buffer(OP_PETICION_IO_STDIN, peticion, peticion_size, io->socket_io);
  pthread_mutex_unlock(&(io->mutex_io));

  // recibo la respuesta de IO
  char* buffer = recibir_string(io->socket_io);

  if (buffer == NULL)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## Error al recibir la respuesa de IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }
  // Le envio el paquete al Kernel Memory para que escriba en la memoria
  pthread_mutex_lock(&(socket_km->mutex_socket));
  enviar_buffer(OP_ESCRIBIR_EN_MEMORIA, peticion, peticion->tamanio_a_leer,
                socket_km->socket_km);
  bool envio =
      enviar_string(OP_ESCRIBIR_EN_MEMORIA, buffer, socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  if(!envio){
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al enviar la respuesta de IO a Kernel Memory");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }

  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## (%d) - Solicitó syscall: STDIN",
           peticion->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));
  free(buffer);

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

bool std_out(t_pcb* pcb, t_io* io_stdout, t_cola* block, t_cola_ready* ready,
             t_lista* susp_block, t_lista* susp_ready,
             t_peticion_stdout* peticion, t_logger* logger,
             t_socket_kernel_memory* socket_km)
{
  // Envio peticion a Kernel Memory para que lea de la memoria
  int peticion_size = sizeof(t_peticion_stdout);
  pthread_mutex_lock(&(io->mutex_io));
  enviar_buffer(OP_PETICION_IO_STDOUT, peticion, peticion_size, io->socket_io);
  pthread_mutex_unlock(&(io->mutex_io));

  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## (%d) - Solicitó syscall: STDOUT ",
           peticion->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));

  //Recibo la respuesta de Kernel Memory
    char* buffer = recibir_string(io->socket_io);
  if (buffer == NULL)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## Error al recibir la respuesa de Kernel Memory");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }
  // Le envio el paquete a IO para que imprima por pantalla
  pthread_mutex_lock(&(io->mutex_io));
  bool envio = enviar_string(OP_RESPUESTA_STDIN, buffer, io->socket_io);
  pthread_mutex_unlock(&(io->mutex_io));
  free(buffer);
  if(!envio){
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al enviar la respuesta de Kernel Memory a IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }
  pthread_mutex_lock(&(io->mutex_io));
  char* resp_io = recibir_string(io->socket_io);
  pthread_mutex_unlock(&(io->mutex_io));
  if (strcmp(resp_io, "OK") != 0)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    pthread_mutex_unlock(&(logger->mutex_logger));
    free(resp_io);
    return false;
  }
  free(resp_io);
  if(!retirar_lista_io(pcb, io_stdout->cola_io, logger))
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

bool io_sleep(t_pcb* pcb, t_io* io_sleep, t_cola* block, t_cola_ready* ready,
             t_lista* susp_block, t_lista* susp_ready,
             t_peticion_sleep* peticion, t_logger* logger)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## (%d) - Solicitó syscall: SLEEP", peticion->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));

  int peticion_size = sizeof(t_peticion_sleep);
  pthread_mutex_lock(&(io->mutex_io));
  enviar_buffer(OP_PETICION_IO_SLEEP, peticion, peticion_size, io->socket_io);
  pthread_mutex_unlock(&(io->mutex_io));

  char* respuesta = recibir_string(io->socket_io);
  if (strcmp(respuesta, "OK") != 0)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    pthread_mutex_unlock(&(logger->mutex_logger));
    free(respuesta);
    return false;
  }
  free(respuesta);

  if(!retirar_lista_io(pcb, io_sleep->cola_io, logger))
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