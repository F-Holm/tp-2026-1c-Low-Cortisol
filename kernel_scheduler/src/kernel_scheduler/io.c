
#include "kernel_scheduler/io.h"

#include <string.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/io.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/msg.h"
#include "utils/registros.h"

t_listas_io* inicializar_listas_io(void)
{
  t_listas_io* listas_io;
  listas_io = malloc(sizeof(t_listas_io));
  listas_io->lista_stdin = malloc(sizeof(t_lista_stdin));
  listas_io->lista_stdout = malloc(sizeof(t_lista_stdout));
  listas_io->lista_sleep = malloc(sizeof(t_lista_sleep));
  listas_io->lista_stdin->lista_stdin = list_create();
  listas_io->lista_stdout->lista_stdout = list_create();
  listas_io->lista_sleep->lista_sleep = list_create();
  return listas_io;
}

int io_stdin(t_stdin* peticion, t_hilo_io_in* hilo_in)
{
  // Envio peticion a IO
  int peticion_size = sizeof(t_peticion_stdin);

  bool envio = enviar_buffer(OP_PETICION_IO_STDIN, peticion->peticion,
                             peticion_size, hilo_in->io->socket_io);
  if (!envio)
  {
    pthread_mutex_lock(&(hilo_in->io->logger->mutex_logger));
    log_error(hilo_in->io->logger->logger, "## Error em el envio a IO");
    pthread_mutex_unlock(&(hilo_in->io->logger->mutex_logger));

    return D_ERROR_IO;
  }
  pthread_mutex_lock(&(hilo_in->io->logger->mutex_logger));
  log_info(hilo_in->io->logger->logger, "## (%d) - Solicitó syscall: STDIN",
           peticion->peticion->pid);
  pthread_mutex_unlock(&(hilo_in->io->logger->mutex_logger));

  // recibo la respuesta de IO
  int cod_op = recibir_operacion(hilo_in->io->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    return D_ERROR_IO;
  }
  char* buffer = recibir_string(hilo_in->io->socket_io);

  if (buffer == NULL)
  {
    pthread_mutex_lock(&(hilo_in->io->logger->mutex_logger));
    log_error(hilo_in->io->logger->logger,
              "## Error al recibir la respuesa de IO");
    pthread_mutex_unlock(&(hilo_in->io->logger->mutex_logger));
    return D_ERROR_IO;
  }
  // Le envio el paquete al Kernel Memory para que escriba en la memoria
  t_paquete* paquete = crear_paquete(OP_PETICION_IO_STDIN);
  agregar_string_a_paquete(paquete, buffer);
  agregar_a_paquete(paquete, peticion->peticion, peticion_size);
  pthread_mutex_lock(&(hilo_in->io->socket_km->mutex_socket));
  envio = enviar_paquete(paquete, hilo_in->io->socket_km->socket_km);
  if (!envio)
  {
    pthread_mutex_lock(&(hilo_in->io->logger->mutex_logger));
    log_error(hilo_in->io->logger->logger,
              "## Error en el envio a Kernel memory");
    pthread_mutex_unlock(&(hilo_in->io->logger->mutex_logger));
    return D_ERROR_CONEXION_KM;
  }

  pthread_mutex_unlock(&(hilo_in->io->socket_km->mutex_socket));
  free(buffer);

  pthread_mutex_lock(&(hilo_in->io->socket_km->mutex_socket));
  cod_op = recibir_operacion(hilo_in->io->socket_km->socket_km);
  if (cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(hilo_in->io->socket_km->mutex_socket));
    return D_ERROR_KM;
  }
  if (cod_op == OP_CODE_ERROR)
  {
    pthread_mutex_unlock(&(hilo_in->io->socket_km->mutex_socket));
    return D_ERROR_CONEXION_KM;
  }
  char* respuesta = recibir_string(hilo_in->io->socket_km->socket_km);
  pthread_mutex_unlock(&(hilo_in->io->socket_km->mutex_socket));
  free(respuesta);

  pthread_mutex_lock(&(hilo_in->lista_stdin->mutex_lista_stdin));
  if (list_remove_element(hilo_in->lista_stdin->lista_stdin, peticion) == 0)
  {
    pthread_mutex_unlock(&(hilo_in->lista_stdin->mutex_lista_stdin));
    pthread_mutex_lock(&(hilo_in->io->logger->mutex_logger));
    log_error(hilo_in->io->logger->logger,
              "## Error al retirar el proceso de la lista de IO");
    pthread_mutex_unlock(&(hilo_in->io->logger->mutex_logger));
    return D_ERROR_IO;
  }
  pthread_mutex_unlock(&(hilo_in->lista_stdin->mutex_lista_stdin));
  pthread_mutex_lock(&(hilo_in->io->logger->mutex_logger));
  log_info(hilo_in->io->logger->logger,
           "## PID %d - Retirado de la lista de IO", peticion->pcb->pid);
  pthread_mutex_unlock(&(hilo_in->io->logger->mutex_logger));

  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  if (hilo_in->io->proceso_actual->tiempo_bloqueado == 0)
  {
    cambio_susp_block_susp_ready(peticion->pcb, hilo_in->io->susp_block,
                                 hilo_in->io->susp_ready, hilo_in->io->logger);
  }
  else
  {
    cambio_block_ready(peticion->pcb, hilo_in->io->cola_block,
                       hilo_in->io->cola_ready, hilo_in->io->logger);
  }
  pthread_mutex_lock(&(hilo_in->io->logger->mutex_logger));
  log_info(hilo_in->io->logger->logger,
           "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
           peticion->pcb->pid);
  pthread_mutex_unlock(&(hilo_in->io->logger->mutex_logger));
  free(paquete);
  return D_TODO_BIEN;
}

int io_stdout(t_stdout* peticion, t_hilo_io_out* hilo_out)
{
  // Envio peticion a Kernel Memory para que lea de la memoria
  int peticion_size = sizeof(t_peticion_stdout);
  pthread_mutex_lock(&(hilo_out->io->socket_km->mutex_socket));
  bool envio = enviar_buffer(OP_PETICION_IO_STDOUT, peticion->peticion,
                             peticion_size, hilo_out->io->socket_km->socket_km);
  if (!envio)
  {
    pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
    log_error(hilo_out->io->logger->logger,
              "## Error en la comunicacion con el Kernel Memory");
    pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
    return D_ERROR_CONEXION_KM;
    pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
  }

  pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));

  pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
  log_info(hilo_out->io->logger->logger, "## (%d) - Solicitó syscall: STDOUT ",
           peticion->peticion->pid);
  pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));

  // Recibo la respuesta de Kernel Memory
  pthread_mutex_lock(&(hilo_out->io->socket_km->mutex_socket));
  int cod_op = recibir_operacion(hilo_out->io->socket_km->socket_km);
  if (cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
    return D_ERROR_KM;
  }
  if (cod_op == OP_CODE_ERROR)
  {
    pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
    return D_ERROR_CONEXION_KM;
  }
  char* buffer = recibir_string(hilo_out->io->socket_km->socket_km);
  pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
  if (buffer == NULL)
  {
    pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
    log_error(hilo_out->io->logger->logger,
              "## Error al recibir la respuesa de Kernel Memory");
    pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
    return D_ERROR_CONEXION_KM;
  }
  // Le envio el mensaje + la peticion a IO para que imprima por pantalla
  enviar_buffer(OP_PETICION_IO_STDOUT, peticion, peticion_size,
                hilo_out->io->socket_io);
  envio = enviar_string(OP_RESPUESTA_STDOUT, buffer, hilo_out->io->socket_io);

  free(buffer);
  if (!envio)
  {
    pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
    log_error(hilo_out->io->logger->logger,
              "## Error al enviar la respuesta de Kernel Memory a IO");
    pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
    return D_ERROR_IO;
  }
  cod_op = recibir_operacion(hilo_out->io->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    return D_ERROR_IO;
  }
  char* resp_io = recibir_string(hilo_out->io->socket_io);

  if (strcmp(resp_io, "OK") != 0)
  {
    pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
    log_error(
        hilo_out->io->logger->logger,
        "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
    free(resp_io);
    return D_ERROR_IO;
  }
  free(resp_io);
  pthread_mutex_lock(&(hilo_out->lista_stdout->mutex_lista_stdout));
  if (list_remove_element(hilo_out->lista_stdout->lista_stdout, peticion) == 0)
  {
    pthread_mutex_unlock(&(hilo_out->lista_stdout->mutex_lista_stdout));
    pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
    log_error(hilo_out->io->logger->logger,
              "## Error al retirar el proceso de la lista de IO");
    pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
    return D_ERROR_IO;
  }
  pthread_mutex_unlock(&(hilo_out->lista_stdout->mutex_lista_stdout));
  pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
  log_info(hilo_out->io->logger->logger,
           "## PID %d - Retirado de la lista de IO", peticion->pcb->pid);
  pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  if (peticion->pcb->tiempo_bloqueado == 0)
  {
    cambio_susp_block_susp_ready(peticion->pcb, hilo_out->io->susp_block,
                                 hilo_out->io->susp_ready,
                                 hilo_out->io->logger);
  }
  else
  {
    cambio_block_ready(peticion->pcb, hilo_out->io->cola_block,
                       hilo_out->io->cola_ready, hilo_out->io->logger);
  }
  pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
  log_info(hilo_out->io->logger->logger,
           "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
           peticion->pcb->pid);
  pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
  return D_TODO_BIEN;
}

int io_sleep(t_sleep* peticion, t_hilo_io_sleep* hilo_sleep)
{
  pthread_mutex_lock(&(hilo_sleep->io->logger->mutex_logger));
  log_info(hilo_sleep->io->logger->logger, "## (%d) - Solicitó syscall: SLEEP",
           peticion->peticion->pid);
  pthread_mutex_unlock(&(hilo_sleep->io->logger->mutex_logger));

  int peticion_size = sizeof(t_peticion_sleep);
  bool envio = enviar_buffer(OP_PETICION_IO_SLEEP, peticion->peticion,
                             peticion_size, hilo_sleep->io->socket_io);
  if (!envio)
  {
    pthread_mutex_lock(&(hilo_sleep->io->logger->mutex_logger));
    log_error(hilo_sleep->io->logger->logger, "## Error al enviar a IO");
    pthread_mutex_unlock(&(hilo_sleep->io->logger->mutex_logger));
    return D_ERROR_IO;
  }

  int cod_op = recibir_operacion(hilo_sleep->io->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    pthread_mutex_lock(&(hilo_sleep->io->logger->mutex_logger));
    log_error(hilo_sleep->io->logger->logger,
              "## Error en la respuesta de IO a Kernel Scheduler");
    pthread_mutex_unlock(&(hilo_sleep->io->logger->mutex_logger));
    return D_ERROR_IO;
  }
  char* respuesta = recibir_string(hilo_sleep->io->socket_io);
  if (strcmp(respuesta, "OK") != 0)
  {
    pthread_mutex_lock(&(hilo_sleep->io->logger->mutex_logger));
    log_error(
        hilo_sleep->io->logger->logger,
        "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    pthread_mutex_unlock(&(hilo_sleep->io->logger->mutex_logger));
    free(respuesta);
    return D_ERROR_IO;
  }
  free(respuesta);

  pthread_mutex_lock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
  if (list_remove_element(hilo_sleep->lista_sleep->lista_sleep, peticion) == 0)
  {
    pthread_mutex_unlock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
    pthread_mutex_lock(&(hilo_sleep->io->logger->mutex_logger));
    log_error(hilo_sleep->io->logger->logger,
              "## Error al retirar el proceso de la lista de IO");
    pthread_mutex_unlock(&(hilo_sleep->io->logger->mutex_logger));
    return D_ERROR_IO;
  }
  pthread_mutex_unlock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
  pthread_mutex_lock(&(hilo_sleep->io->logger->mutex_logger));
  log_info(hilo_sleep->io->logger->logger,
           "## PID %d - Retirado de la lista de IO", peticion->pcb->pid);
  pthread_mutex_unlock(&(hilo_sleep->io->logger->mutex_logger));
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  if (peticion->pcb->tiempo_bloqueado == 0)
  {
    cambio_susp_block_susp_ready(peticion->pcb, hilo_sleep->io->susp_block,
                                 hilo_sleep->io->susp_ready,
                                 hilo_sleep->io->logger);
  }
  else
  {
    cambio_block_ready(peticion->pcb, hilo_sleep->io->cola_block,
                       hilo_sleep->io->cola_ready, hilo_sleep->io->logger);
  }
  pthread_mutex_lock(&(hilo_sleep->io->logger->mutex_logger));
  log_info(hilo_sleep->io->logger->logger,
           "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
           peticion->pcb->pid);
  pthread_mutex_unlock(&(hilo_sleep->io->logger->mutex_logger));
  return D_TODO_BIEN;
}

void* hilo_io_in(void* hilo_in)
{
  t_hilo_io_in* hilo_stdin = (t_hilo_io_in*)hilo_in;
  pthread_mutex_lock(&(hilo_stdin->io->mutex_fin));
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
    while (list_is_empty(hilo_stdin->lista_stdin->lista_stdin))
    {
      pthread_cond_wait(&(hilo_stdin->io->nuevo_proceso),
                        &(hilo_stdin->lista_stdin->mutex_lista_stdin));
    }

    pthread_mutex_unlock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));

    t_stdin* peticion = NULL;
    pthread_mutex_lock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
    peticion = list_get(hilo_stdin->lista_stdin->lista_stdin, 0);
    pthread_mutex_unlock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
    if (peticion == NULL)
    {
      pthread_mutex_lock(&(hilo_stdin->io->logger->mutex_logger));
      log_error(hilo_stdin->io->logger->logger,
                "## Error al obtener la peticion de la lista de stdin");
      pthread_mutex_unlock(&(hilo_stdin->io->logger->mutex_logger));
      seguir_atendiendo = false;
    }
    int stdin = io_stdin(peticion, hilo_stdin);
    if (D_ERROR_KM == stdin)
    {
      cerrar_kernel_scheduler(hilo_stdin->io->socket_server,
                              hilo_stdin->io->logger, MC_MEMORIA_CORRUPTA);
      continue;
    }
    else
    {
      if (D_ERROR_IO == stdin)
      {
        pthread_mutex_lock(&(hilo_stdin->io->logger->mutex_logger));
        log_error(hilo_stdin->io->logger->logger,
                  "## Error al atender la petición de stdin");
        pthread_mutex_unlock(&(hilo_stdin->io->logger->mutex_logger));
        seguir_atendiendo = false;
      }
      else
      {
        if (D_ERROR_CONEXION_KM == stdin)

          cerrar_kernel_scheduler(hilo_stdin->io->socket_server,
                                  hilo_stdin->io->logger,
                                  MC_FALLO_CONEXION_KERNEL_MEMORY);
        continue;
      }
    }
    if (hilo_stdin->io->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  pthread_mutex_lock(&(hilo_stdin->io->mutex_socket_io));
  close(hilo_stdin->io->socket_io);
  hilo_stdin->io->socket_io = -1;
  pthread_mutex_unlock(&(hilo_stdin->io->mutex_socket_io));
  pthread_mutex_lock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
  while (!list_is_empty(hilo_stdin->lista_stdin->lista_stdin))
  {
    t_stdin* peticion = list_remove(hilo_stdin->lista_stdin->lista_stdin, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, hilo_stdin->io->cola_block,
                       hilo_stdin->io->susp_block, hilo_stdin->io->susp_ready,
                       hilo_stdin->io->cola_ready, hilo_stdin->io->logger);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
  pthread_mutex_destroy(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
  list_destroy(hilo_stdin->lista_stdin->lista_stdin);
  free(hilo_stdin->lista_stdin);
  pthread_mutex_unlock(&(hilo_stdin->io->mutex_fin));
  free(hilo_stdin);
  return NULL;
}

void* hilo_io_out(void* hilo_out)
{
  t_hilo_io_out* hilo_stdout = (t_hilo_io_out*)hilo_out;

  pthread_mutex_lock(&(hilo_stdout->io->mutex_fin));
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
    while (list_is_empty(hilo_stdout->lista_stdout->lista_stdout))
    {
      pthread_cond_wait(&(hilo_stdout->io->nuevo_proceso),
                        &(hilo_stdout->lista_stdout->mutex_lista_stdout));
    }

    pthread_mutex_unlock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
    t_stdout* peticion = NULL;
    pthread_mutex_lock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
    peticion = list_get(hilo_stdout->lista_stdout->lista_stdout, 0);
    pthread_mutex_unlock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
    if (peticion == NULL)
    {
      pthread_mutex_lock(&(hilo_stdout->io->logger->mutex_logger));
      log_error(hilo_stdout->io->logger->logger,
                "## Error al obtener la peticion de la lista de stdout");
      pthread_mutex_unlock(&(hilo_stdout->io->logger->mutex_logger));
      continue;
    }
    int op_stdout = io_stdout(peticion, hilo_stdout);
    if (D_ERROR_IO == op_stdout)
    {
      pthread_mutex_lock(&(hilo_stdout->io->logger->mutex_logger));
      log_error(hilo_stdout->io->logger->logger,
                "## Error al atender la petición de stdout");
      pthread_mutex_unlock(&(hilo_stdout->io->logger->mutex_logger));
      seguir_atendiendo = false;
    }
    else
    {
      if (D_ERROR_KM == op_stdout)
      {
        cerrar_kernel_scheduler(hilo_stdout->io->socket_server,
                                hilo_stdout->io->logger, MC_MEMORIA_CORRUPTA);
        continue;
      }
      else
      {
        if (D_ERROR_CONEXION_KM == op_stdout)
        {
          cerrar_kernel_scheduler(hilo_stdout->io->socket_server,
                                  hilo_stdout->io->logger,
                                  MC_FALLO_CONEXION_KERNEL_MEMORY);
          continue;
        }
      }
    }

    if (hilo_stdout->io->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  pthread_mutex_lock(&(hilo_stdout->io->mutex_socket_io));
  close(hilo_stdout->io->socket_io);
  hilo_stdout->io->socket_io = -1;
  pthread_mutex_unlock(&(hilo_stdout->io->mutex_socket_io));

  // cierro la lista de stdout
  pthread_mutex_lock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
  while (!list_is_empty(hilo_stdout->lista_stdout->lista_stdout))
  {
    t_stdout* peticion =
        list_remove(hilo_stdout->lista_stdout->lista_stdout, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, hilo_stdout->io->cola_block,
                       hilo_stdout->io->susp_block, hilo_stdout->io->susp_ready,
                       hilo_stdout->io->cola_ready, hilo_stdout->io->logger);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
  pthread_mutex_destroy(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
  list_destroy(hilo_stdout->lista_stdout->lista_stdout);
  free(hilo_stdout->lista_stdout);
  pthread_mutex_unlock(&(hilo_stdout->io->mutex_fin));
  free(hilo_stdout);
  return NULL;
}

void* hilo_io_sleep(void* hilo_sleep)
{
  t_hilo_io_sleep* shilo_sleep = (t_hilo_io_sleep*)hilo_sleep;
  pthread_mutex_lock(&(shilo_sleep->io->mutex_fin));
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
    while (list_is_empty(shilo_sleep->lista_sleep->lista_sleep))
    {
      pthread_cond_wait(&(shilo_sleep->io->nuevo_proceso),
                        &(shilo_sleep->lista_sleep->mutex_lista_sleep));
    }

    pthread_mutex_unlock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
    t_sleep* peticion = NULL;
    pthread_mutex_lock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
    peticion = list_get(shilo_sleep->lista_sleep->lista_sleep, 0);
    pthread_mutex_unlock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
    if (peticion == NULL)
    {
      pthread_mutex_lock(&(shilo_sleep->io->logger->mutex_logger));
      log_error(shilo_sleep->io->logger->logger,
                "## Error al obtener la peticion de la lista de sleep");
      pthread_mutex_unlock(&(shilo_sleep->io->logger->mutex_logger));
      continue;
    }
    if (D_ERROR_IO == io_sleep(peticion, shilo_sleep))
    {
      pthread_mutex_lock(&(shilo_sleep->io->logger->mutex_logger));
      log_error(shilo_sleep->io->logger->logger,
                "## Error al atender la petición de sleep");
      pthread_mutex_unlock(&(shilo_sleep->io->logger->mutex_logger));
      seguir_atendiendo = false;
    }

    if (shilo_sleep->io->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  pthread_mutex_lock(&(shilo_sleep->io->mutex_socket_io));
  close(shilo_sleep->io->socket_io);
  shilo_sleep->io->socket_io = -1;
  pthread_mutex_unlock(&(shilo_sleep->io->mutex_socket_io));

  pthread_mutex_lock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
  while (!list_is_empty(shilo_sleep->lista_sleep->lista_sleep))
  {
    t_sleep* peticion = list_remove(shilo_sleep->lista_sleep->lista_sleep, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, shilo_sleep->io->cola_block,
                       shilo_sleep->io->susp_block, shilo_sleep->io->susp_ready,
                       shilo_sleep->io->cola_ready, shilo_sleep->io->logger);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
  pthread_mutex_destroy(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
  list_destroy(shilo_sleep->lista_sleep->lista_sleep);
  free(shilo_sleep->lista_sleep);
  pthread_mutex_unlock(&(shilo_sleep->io->mutex_fin));
  free(shilo_sleep);
  return NULL;
}

int obtener_tipo_io(int socket_fd, t_logger* logger)
{
  if (recibir_operacion(socket_fd) != OP_TIPO_IO)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error en el tipo de operación. Expected: OP_TIPO_IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
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
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## Tipo de IO no válido: %s", buffer);
    pthread_mutex_unlock(&(logger->mutex_logger));
    free(buffer);
    return -1;
  }
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## IO de tipo %s conectada", buffer);
  pthread_mutex_unlock(&(logger->mutex_logger));
  free(buffer);
  return tipo_io;
}

void cargar_stdin(t_io* io, t_lista_stdin* lista_stdin, t_hilo_io_in* hilo_in)
{
  hilo_in->io = io;
  hilo_in->lista_stdin = lista_stdin;
}
void cargar_stdout(t_io* io, t_lista_stdout* lista_stdout,
                   t_hilo_io_out* hilo_out)
{
  hilo_out->io = io;
  hilo_out->lista_stdout = lista_stdout;
}
void cargar_sleep(t_io* io, t_lista_sleep* lista_sleep,
                  t_hilo_io_sleep* hilo_sleep)
{
  hilo_sleep->io = io;
  hilo_sleep->lista_sleep = lista_sleep;
}

bool atender_nuevo_io(t_io io[3], int socket_fd, t_logger* logger,
                      t_socket_kernel_memory* socket_km, t_lista* block,
                      t_cola_ready* ready, t_lista* susp_block,
                      t_lista* susp_ready, t_listas_io* listas_io)
{
  if (!responder_handshake(socket_fd, MID_KERNEL_SCHEDULER, logger))
    return false;

  int tipo_io = obtener_tipo_io(socket_fd, logger);
  if (tipo_io == -1)
    return false;

  if (io[tipo_io].socket_io != -1)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## IO de tipo repetido: %d. Cerrando conexión",
              tipo_io);
    pthread_mutex_unlock(&(logger->mutex_logger));
    close(socket_fd);
    return false;
  }
  // Preparo el t_io para crear el hilo
  io[tipo_io].socket_io = socket_fd;
  io[tipo_io].proceso_actual = NULL;
  pthread_mutex_init(&(io[tipo_io].mutex_fin), NULL);
  pthread_cond_init(&(io[tipo_io].nuevo_proceso), NULL);
  io[tipo_io].cola_ready = ready;
  io[tipo_io].cola_block = block;
  io[tipo_io].susp_block = susp_block;
  io[tipo_io].susp_ready = susp_ready;
  io[tipo_io].logger = logger;
  io[tipo_io].socket_km = socket_km;

  switch (tipo_io)
  {
    case E_STDIN:
      t_hilo_io_in* hilo_in;
      hilo_in = malloc(sizeof(t_hilo_io_in));
      cargar_stdin(&io[E_STDIN], listas_io->lista_stdin, hilo_in);
      pthread_mutex_init(&(hilo_in->lista_stdin->mutex_lista_stdin), NULL);
      if (pthread_create(&(hilo_in->io->hilo_io), NULL, hilo_io_in,
                         (void*)hilo_in))
      {
        pthread_mutex_lock(&(logger->mutex_logger));
        log_error(logger->logger,
                  "## Error al crear el hilo para IO de tipo stdin");
        pthread_mutex_unlock(&(logger->mutex_logger));
        return false;
      }
      break;
    case E_STDOUT:
      t_hilo_io_out* hilo_stdout;
      hilo_stdout = malloc(sizeof(t_hilo_io_out));
      cargar_stdout(&io[E_STDOUT], listas_io->lista_stdout, hilo_stdout);
      pthread_mutex_init(&(hilo_stdout->lista_stdout->mutex_lista_stdout),
                         NULL);
      if (pthread_create(&(hilo_stdout->io->hilo_io), NULL, hilo_io_out,
                         (void*)hilo_stdout))
      {
        pthread_mutex_lock(&(logger->mutex_logger));
        log_error(logger->logger,
                  "## Error al crear el hilo para IO de tipo stdout");
        pthread_mutex_unlock(&(logger->mutex_logger));
        return false;
      }
      break;
    case E_SLEEP:
      t_hilo_io_sleep* hilo_sleep;
      hilo_sleep = malloc(sizeof(t_hilo_io_sleep));
      cargar_sleep(&io[E_SLEEP], listas_io->lista_sleep, hilo_sleep);
      pthread_mutex_init(&(hilo_sleep->lista_sleep->mutex_lista_sleep), NULL);
      if (pthread_create(&(hilo_sleep->io->hilo_io), NULL, hilo_io_sleep,
                         (void*)hilo_sleep))
      {
        pthread_mutex_lock(&(logger->mutex_logger));
        log_error(logger->logger,
                  "## Error al crear el hilo para IO de tipo sleep");
        pthread_mutex_unlock(&(logger->mutex_logger));
        return false;
      }
      break;
  }

  return true;
}

bool procesar_nuevo_stdin(t_peticion_stdin* peticion, t_io* io_stdin,
                          t_lista_stdin* lista_stdin)
{
  pthread_mutex_lock(&(io_stdin->mutex_socket_io));
  if (io_stdin->socket_io == -1)
  {
    pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
    return false;
  }

  pthread_mutex_lock(&(lista_stdin->mutex_lista_stdin));
  bool lista_vacia = list_is_empty(lista_stdin->lista_stdin);
  list_add(lista_stdin->lista_stdin, peticion);
  pthread_mutex_unlock(&(lista_stdin->mutex_lista_stdin));
  pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
  if (lista_vacia)
  {
    pthread_cond_signal(&(io_stdin->nuevo_proceso));
  }
  return true;
}

bool procesar_nuevo_stdout(t_peticion_stdout* peticion, t_io* io_stdout,
                           t_lista_stdout* lista_stdout, t_logger* logger)
{
  pthread_mutex_lock(&(io_stdout->mutex_socket_io));
  if (io_stdout->socket_io == -1)
  {
    pthread_mutex_unlock(&(io_stdout->mutex_socket_io));
    return false;
  }

  pthread_mutex_lock(&(lista_stdout->mutex_lista_stdout));
  bool lista_vacia = list_is_empty(lista_stdout->lista_stdout);
  list_add(lista_stdout->lista_stdout, peticion);
  pthread_mutex_unlock(&(lista_stdout->mutex_lista_stdout));
  pthread_mutex_unlock(&(io_stdout->mutex_socket_io));

  if (lista_vacia)
  {
    pthread_cond_signal(&(io_stdout->nuevo_proceso));
  }
  return true;
}

bool procesar_nuevo_sleep(t_peticion_sleep* peticion, t_io* io_sleep,
                          t_lista_sleep* lista_sleep, t_logger* logger)
{
  pthread_mutex_lock(&(io_sleep->mutex_socket_io));
  if (io_sleep->socket_io == -1)
  {
    pthread_mutex_unlock(&(io_sleep->mutex_socket_io));
    return false;
  }
  pthread_mutex_lock(&(lista_sleep->mutex_lista_sleep));
  if (list_is_empty(lista_sleep->lista_sleep))
  {
    pthread_cond_signal(&(io_sleep->nuevo_proceso));
  }
  list_add(lista_sleep->lista_sleep, peticion);
  pthread_mutex_unlock(&(lista_sleep->mutex_lista_sleep));
  pthread_mutex_unlock(&(io_sleep->mutex_socket_io));
  return true;
}

void destruir_io(t_io* io)
{
  pthread_mutex_destroy(&(io->mutex_socket_io));
  pthread_mutex_destroy(&(io->mutex_fin));
  pthread_cond_destroy(&(io->nuevo_proceso));
  close(io->socket_io);
}

void cerrar_io(t_io io[3], t_listas_io* listas_io)
{
  for (int i = 0; i < 3; i++)
  {
    io[i].cerrar_hilo = true;
    pthread_mutex_lock(&(io[i].mutex_fin));
    destruir_io(&io[i]);
  }
  free(listas_io);
}
