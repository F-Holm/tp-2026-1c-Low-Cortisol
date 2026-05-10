#include "kernel_scheduler/io.h"

#include <string.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/io.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/msg.h"
#include "utils/registros.h"

bool retirar_lista_io(t_pcb* pcb, t_list* lista_io, t_logger* logger,
                      pthread_mutex_t* mutex_io)
{
  pthread_mutex_lock(mutex_io);
  if (list_remove_element(lista_io, pcb) == 0)
  {
    pthread_mutex_unlock(mutex_io);
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al retirar el proceso de la lista de IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }
  pthread_mutex_unlock(mutex_io);
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## PID %d - Retirado de la lista de IO", pcb->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));
  return true;
}

int io_stdin(t_pcb* pcb, t_io* io_stdin, t_cola* block, t_cola_ready* ready,
             t_lista* susp_block, t_lista* susp_ready,
             t_peticion_stdin* peticion, t_logger* logger,
             t_socket_kernel_memory* socket_km, t_lista_stdin* lista_stdin)
{
  // Envio peticion a IO
  int peticion_size = sizeof(t_peticion_stdin);

  enviar_buffer(OP_PETICION_IO_STDIN, peticion, peticion_size,
                io_stdin->socket_io);

  // recibo la respuesta de IO
  int cod_op = recibir_operacion(io_stdin->socket_io);
  char* buffer = recibir_string(io_stdin->socket_io);

  if (buffer == NULL)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## Error al recibir la respuesa de IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return ERROR_IO;
  }
  // Le envio el paquete al Kernel Memory para que escriba en la memoria
  pthread_mutex_lock(&(socket_km->mutex_socket));
  enviar_buffer(OP_ESCRIBIR_EN_MEMORIA, peticion, peticion->tamanio_a_leer,
                socket_km->socket_km);
  bool envio =
      enviar_string(OP_ESCRIBIR_EN_MEMORIA, buffer, socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  if (!envio)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al enviar la respuesta de IO a Kernel Memory");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return ERROR_IO;
  }

  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## (%d) - Solicitó syscall: STDIN", peticion->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));
  free(buffer);

  pthread_mutex_lock(&(socket_km->mutex_socket));
  cod_op = recibir_operacion(socket_km->socket_km);
  if (cod_op == OP_CODE_ERROR || cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(socket_km->mutex_socket));
    return ERROR_KM;
  }
  char* respuesta = recibir_string(socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  free(respuesta);
  free(buffer);

  if (!retirar_lista_io(pcb, lista_stdin->lista_stdin, logger,
                        &(lista_stdin->mutex_lista_stdin)))
  {
    return ERROR_IO;
  }
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  if (pcb->tiempo_bloqueado == 0)
  {
    cambio_susp_block_susp_ready(pcb, susp_block, susp_ready, logger);
  }
  else
  {
    cambio_block_ready(pcb, block, ready, logger);
  }

  return TODO_BIEN;
}

int io_stdout(t_pcb* pcb, t_io* io_stdout, t_cola* block, t_cola_ready* ready,
              t_lista* susp_block, t_lista* susp_ready,
              t_peticion_stdout* peticion, t_logger* logger,
              t_socket_kernel_memory* socket_km, t_lista_stdout* lista_stdout)
{
  // Envio peticion a Kernel Memory para que lea de la memoria
  int peticion_size = sizeof(t_peticion_stdout);
  pthread_mutex_lock(&(socket_km->mutex_socket));
  enviar_buffer(OP_PETICION_IO_STDOUT, peticion, peticion_size,
                socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));

  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## (%d) - Solicitó syscall: STDOUT ",
           peticion->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));

  // Recibo la respuesta de Kernel Memory
  pthread_mutex_lock(&(socket_km->mutex_socket));
  int cod_op = recibir_operacion(socket_km->socket_km);
  if (cod_op == OP_CODE_ERROR || cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(socket_km->mutex_socket));
    return ERROR_KM;
  }
  char* buffer = recibir_string(socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  if (buffer == NULL)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al recibir la respuesa de Kernel Memory");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return ERROR_IO;
  }
  // Le envio el mensaje + la peticion a IO para que imprima por pantalla
  enviar_buffer(OP_PETICION_IO_STDOUT, peticion, peticion_size,
                io_stdout->socket_io);
  bool envio = enviar_string(OP_RESPUESTA_STDOUT, buffer, io_stdout->socket_io);

  free(buffer);
  if (!envio)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error al enviar la respuesta de Kernel Memory a IO");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return ERROR_IO;
  }
  cod_op = recibir_operacion(io_stdout->socket_io);
  char* resp_io = recibir_string(io_stdout->socket_io);

  if (strcmp(resp_io, "OK") != 0)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(
        logger->logger,
        "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    pthread_mutex_unlock(&(logger->mutex_logger));
    free(resp_io);
    return ERROR_IO;
  }
  free(resp_io);
  if (!retirar_lista_io(pcb, lista_stdout->lista_stdout, logger,
                        &(lista_stdout->mutex_lista_stdout)))
  {
    return ERROR_IO;
  }
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  if (pcb->tiempo_bloqueado == 0)
  {
    cambio_susp_block_susp_ready(pcb, susp_block, susp_ready, logger);
  }
  else
  {
    cambio_block_ready(pcb, block, ready, logger);
  }
  return TODO_BIEN;
}

int io_sleep(t_pcb* pcb, t_io* io_sleep, t_cola* block, t_cola_ready* ready,
             t_lista* susp_block, t_lista* susp_ready,
             t_peticion_sleep* peticion, t_logger* logger,
             t_lista_sleep* lista_sleep)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## (%d) - Solicitó syscall: SLEEP", peticion->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));

  int peticion_size = sizeof(t_peticion_sleep);
  enviar_buffer(OP_PETICION_IO_SLEEP, peticion, peticion_size,
                io_sleep->socket_io);

  int cod_op = recibir_operacion(io_sleep->socket_io);
  char* respuesta = recibir_string(io_sleep->socket_io);
  if (strcmp(respuesta, "OK") != 0)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(
        logger->logger,
        "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    pthread_mutex_unlock(&(logger->mutex_logger));
    free(respuesta);
    return ERROR_IO;
  }
  free(respuesta);

  if (!retirar_lista_io(pcb, lista_sleep->lista_sleep, logger,
                        &(lista_sleep->mutex_lista_sleep)))
  {
    return ERROR_IO;
  }
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  if (pcb->tiempo_bloqueado == 0)
  {
    cambio_susp_block_susp_ready(pcb, susp_block, susp_ready, logger);
  }
  else
  {
    cambio_block_ready(pcb, block, ready, logger);
  }
  return TODO_BIEN;
}

void* hilo_stdin(void* io, void* logger, void* socket_km,
                 void* lista_stdin)
{
   t_io* sio = (t_io*)io;
  t_logger* plogger = (t_logger*)logger;
  t_socket_kernel_memory* psocket_km = (t_socket_kernel_memory*)socket_km;
  t_lista_stdin* slista_stdin = (t_lista_stdin*) lista_stdin;
  pthread_mutex_lock(&(sio->mutex_fin));
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(slista_stdin->mutex_lista_stdin));
    while (list_is_empty(slista_stdin->lista_stdin))
    {
      pthread_cond_wait(&(sio->nuevo_proceso),
                        &(slista_stdin->mutex_lista_stdin));
    }

    pthread_mutex_unlock(&(slista_stdin->mutex_lista_stdin));

    t_stdin* peticion = malloc(sizeof(t_stdin));
    pthread_mutex_lock(&(slista_stdin->mutex_lista_stdin));
    peticion = list_get(slista_stdin->lista_stdin, 0);
    pthread_mutex_unlock(&(slista_stdin->mutex_lista_stdin));
    if (peticion == NULL)
    {
      pthread_mutex_lock(&(plogger->mutex_logger));
      log_error(plogger->logger,
                "## Error al obtener la peticion de la lista de stdin");
      pthread_mutex_unlock(&(plogger->mutex_logger));
      seguir_atendiendo = false;
    }
    int stdin = io_stdin(peticion->pcb, sio, sio->cola_block, sio->cola_ready,
                         sio->susp_block, sio->susp_ready, peticion->peticion,
                         plogger, psocket_km, slista_stdin);
    if (ERROR_KM == stdin)
    {
      cerrar_kernel_scheduler(sio->socket_server, plogger, MC_MEMORIA_CORRUPTA);
      continue;
    }
    else
    {
      if (ERROR_IO == stdin)
      {
        pthread_mutex_lock(&(plogger->mutex_logger));
        log_error(plogger->logger, "## Error al atender la petición de stdin");
        pthread_mutex_unlock(&(plogger->mutex_logger));
        seguir_atendiendo = false;
      }
    }
    free(peticion->peticion);
    free(peticion);
    if (io->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  pthread_mutex_lock(&(sio->mutex_socket_io));
  close(sio->socket_io);
  sio->socket_io = -1;
  pthread_mutex_unlock(&(sio->mutex_socket_io));
  pthread_mutex_lock(&(slista_stdin->mutex_lista_stdin));
  while (!list_is_empty(slista_stdin->lista_stdin))
  {
    t_stdin* peticion = list_remove(slista_stdin->lista_stdin, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, sio->cola_block, sio->susp_block, sio->susp_ready,
                       sio->cola_ready, logger);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(slista_stdin->mutex_lista_stdin));
  pthread_mutex_destroy(&(slista_stdin->mutex_lista_stdin));
  list_destroy(slista_stdin->lista_stdin);
  free(slista_stdin);
  pthread_mutex_unlock(&(sio->mutex_fin));
  return NULL;
}

void* hilo_stdout(void* io, void* logger, void* socket_km,
                  void* lista_stdout)
{
  
  t_io* sio = (t_io*)io;
  t_logger* plogger = (t_logger*)logger;
  t_socket_kernel_memory* psocket_km = (t_socket_kernel_memory*)socket_km;
  t_lista_stdout* slista_stdout = (t_lista_stdout*) lista_stdout;
  pthread_mutex_lock(&(sio->mutex_fin));
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(slista_stdout->mutex_lista_stdout));
    while (list_is_empty(slista_stdout->lista_stdout))
    {
      pthread_cond_wait(&(sio->nuevo_proceso),
                        &(slista_stdout->mutex_lista_stdout));
    }

    pthread_mutex_unlock(&(slista_stdout->mutex_lista_stdout));
    t_stdout* peticion = malloc(sizeof(t_stdout));
    pthread_mutex_lock(&(slista_stdout->mutex_lista_stdout));
    peticion = list_get(slista_stdout->lista_stdout, 0);
    pthread_mutex_unlock(&(slista_stdout->mutex_lista_stdout));
    if (peticion == NULL)
    {
      pthread_mutex_lock(&(plogger->mutex_logger));
      log_error(plogger->logger,
                "## Error al obtener la peticion de la lista de stdout");
      pthread_mutex_unlock(&(plogger->mutex_logger));
      continue;
    }
    int op_stdout = io_stdout(
        peticion->pcb, sio, sio->cola_block, sio->cola_ready, sio->susp_block,
        sio->susp_ready, peticion->peticion, plogger, psocket_km, slista_stdout);
    if (ERROR_IO == op_stdout)
    {
      pthread_mutex_lock(&(plogger->mutex_logger));
      log_error(plogger->logger, "## Error al atender la petición de stdout");
      pthread_mutex_unlock(&(plogger->mutex_logger));
      seguir_atendiendo = false;
    }
    else
    {
      if (ERROR_KM == op_stdout)
      {
        cerrar_kernel_scheduler(sio->socket_server, plogger, MC_MEMORIA_CORRUPTA);
        continue;
      }
    }
    free(peticion->peticion);
    free(peticion);
    if (sio->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  pthread_mutex_lock(&(sio->mutex_socket_io));
  close(sio->socket_io);
  sio->socket_io = -1;
  pthread_mutex_unlock(&(sio->mutex_socket_io));

  // cierro la lista de stdout
  pthread_mutex_lock(&(slista_stdout->mutex_lista_stdout));
  while (!list_is_empty(slista_stdout->lista_stdout))
  {
    t_stdout* peticion = list_remove(slista_stdout->lista_stdout, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, sio->cola_block, sio->susp_block, sio->susp_ready,
                       sio->cola_ready, plogger);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(slista_stdout->mutex_lista_stdout));
  pthread_mutex_destroy(&(slista_stdout->mutex_lista_stdout));
  list_destroy(slista_stdout->lista_stdout);
  free(slista_stdout);
  pthread_mutex_unlock(&(sio->mutex_fin));
  return NULL;
}

void* hilo_sleep(void* io, void* logger, 
                 void* lista_sleep)
{
  t_io* sio = (t_io*)io;
  t_logger* plogger = (t_logger*)logger;
  t_lista_sleep* slista_sleep = (t_lista_sleep*) lista_sleep;
  pthread_mutex_lock(&(sio->mutex_fin));
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(slista_sleep->mutex_lista_sleep));
    while (list_is_empty(slista_sleep->lista_sleep))
    {
      pthread_cond_wait(&(sio->nuevo_proceso),
                        &(slista_sleep->mutex_lista_sleep));
    }

    pthread_mutex_unlock(&(slista_sleep->mutex_lista_sleep));
    t_sleep* peticion = malloc(sizeof(t_sleep));
    pthread_mutex_lock(&(slista_sleep->mutex_lista_sleep));
    peticion = list_get(slista_sleep->lista_sleep, 0);
    pthread_mutex_unlock(&(slista_sleep->mutex_lista_sleep));
    if (peticion == NULL)
    {
      pthread_mutex_lock(&(plogger->mutex_logger));
      log_error(plogger->logger,
                "## Error al obtener la peticion de la lista de sleep");
      pthread_mutex_unlock(&(plogger->mutex_logger));
      continue;
    }
    if (ERROR_IO == io_sleep(peticion->pcb, sio, sio->cola_block, sio->cola_ready,
                             sio->susp_block, sio->susp_ready, peticion->peticion,
                             plogger, slista_sleep))
    {
      pthread_mutex_lock(&(plogger->mutex_logger));
      log_error(plogger->logger, "## Error al atender la petición de sleep");
      pthread_mutex_unlock(&(plogger->mutex_logger));
      seguir_atendiendo = false;
    }
    free(peticion->peticion);
    free(peticion);
    if (sio->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  pthread_mutex_lock(&(sio->mutex_socket_io));
  close(sio->socket_io);
  sio->socket_io = -1;
  pthread_mutex_unlock(&(sio->mutex_socket_io));

  pthread_mutex_lock(&(slista_sleep->mutex_lista_sleep));
  while (!list_is_empty(slista_sleep->lista_sleep))
  {
    t_sleep* peticion = list_remove(slista_sleep->lista_sleep, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, sio->cola_block, sio->susp_block, sio->susp_ready,
                       sio->cola_ready, plogger);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(slista_sleep->mutex_lista_sleep));
  pthread_mutex_destroy(&(slista_sleep->mutex_lista_sleep));
  list_destroy(slista_sleep->lista_sleep);
  free(slista_sleep->lista_sleep);
  free(slista_sleep);

  pthread_mutex_unlock(&(sio->mutex_fin));
  return NULL;
}

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

bool atender_nuevo_io(t_io* io[3], int socket_fd, t_logger* logger,
                      t_socket_kernel_memory* socket_km, t_cola* block,
                      t_cola_ready* ready, t_lista* susp_block,
                      t_lista* susp_ready)
{
  if (!responder_handshake(socket_fd, MID_KERNEL_SCHEDULER, logger->logger))
    return false;

  int tipo_io = obtener_tipo_io(socket_fd, logger->logger);
  if (tipo_io == -1)
    return false;

  if (io[tipo_io]->socket_io != -1)
  {
    log_error(logger->logger, "## IO de tipo repetido: %d. Cerrando conexión",
              tipo_io);
    close(socket_fd);
    return false;
  }
  // Preparo el t_io para crear el hilo
  io[tipo_io]->socket_io = socket_fd;
  io[tipo_io]->proceso_actual = NULL;
  pthread_mutex_init(&(io[tipo_io]->mutex_fin), NULL);
  pthread_cond_init(&(io[tipo_io]->nuevo_proceso), NULL);
  io[tipo_io]->cola_ready = ready;
  io[tipo_io]->cola_block = block;
  io[tipo_io]->susp_block = susp_block;
  io[tipo_io]->susp_ready = susp_ready;
  io[tipo_io]->logger = logger;
  io[tipo_io]->socket_km = socket_km;

  switch (tipo_io)
  {
    case E_STDIN:
      t_lista_stdin* lista_stdin;
      lista_stdin->lista_stdin = list_create();
      pthread_mutex_init(&(lista_stdin->mutex_lista_stdin), NULL);
      if (pthread_create(&(io[E_STDIN]->hilo_io), NULL, hilo_stdin, (void*)io[E_STDIN], (void*)socket_km, (void*)logger,
                         (void*)lista_stdin) != 0)
      {
        log_error(logger->logger, "## Error al crear el hilo para IO de tipo stdin");
        return false;
      }
      break;
    case E_STDOUT:
      t_lista_stdout* lista_stdout;
      lista_stdout->lista_stdout = list_create();
      pthread_mutex_init(&(lista_stdout->mutex_lista_stdout), NULL);
      if (pthread_create(&(io[E_STDOUT]->hilo_io), NULL, hilo_stdout,
                         (void*)io[E_STDOUT], (void*)socket_km, (void*)logger,
                         (void*)lista_stdout) != 0)
      {
        log_error(logger->logger, "## Error al crear el hilo para IO de tipo stdout");
        return false;
      }
      break;
    case E_SLEEP:
      t_lista_sleep* lista_sleep;
      lista_sleep->lista_sleep = list_create();
      pthread_mutex_init(&(lista_sleep->mutex_lista_sleep), NULL);
      if (pthread_create(&(io[E_SLEEP]->hilo_io), NULL, hilo_sleep,
                         (void*)io[E_SLEEP], (void*)logger,
                         (void*)lista_sleep) != 0)
      {
        log_error(logger->logger, "## Error al crear el hilo para IO de tipo sleep");
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
    return false;
  }
  

  pthread_mutex_lock(&(lista_sleep->mutex_lista_sleep));
  bool lista_vacia = list_is_empty(lista_sleep->lista_sleep);
  list_add(lista_sleep->lista_sleep, peticion);
  pthread_mutex_unlock(&(lista_sleep->mutex_lista_sleep));
  pthread_mutex_unlock(&(io_sleep->mutex_socket_io));
  if (lista_vacia)
  {
    pthread_cond_signal(&(io_sleep->nuevo_proceso));
  }
  return true;
}

void destruir_io(t_io* io[3])
{
  for (int i = 0; i < 3; i++)
  {
    pthread_mutex_destroy(&(io[i]->mutex_socket_io));
    pthread_mutex_destroy(&(io[i]->mutex_fin));
    pthread_cond_destroy(&(io[i]->nuevo_proceso));
  }
}

void cerrar_io(t_io* io[3])
{
  for (int i = 0; i < 3; i++)
  {
    io[i]->cerrar_hilo = true;
    pthread_mutex_lock(&(io[i]->mutex_fin));
  }
  destruir_io(&io[3]);
  free(io);
}
