
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

// Funciones de comunicacion de syscalls IO
bool envio_stdout(t_hilo_io_out* hilo_out, t_stdout* peticion, char* buffer)
{
  int peticion_size = sizeof(t_peticion_stdout);
  t_paquete* paquete = crear_paquete(OP_PETICION_IO_STDOUT);
  agregar_a_paquete(paquete, peticion->peticion, peticion_size);
  agregar_string_a_paquete(paquete, buffer);
  bool envio = enviar_paquete(paquete, hilo_out->io->socket_io);
  eliminar_paquete(paquete);
  free(buffer);
  if (!envio)
  {
    logger_error(hilo_out->io->logger,
                 "## Error al enviar la respuesta de Kernel Memory a IO");
    return false;
  }
  int cod_op = recibir_operacion(hilo_out->io->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    return false;
  }
  return true;
}

bool peticion_stdout_km(t_stdout* peticion, t_hilo_io_out* hilo_out)
{
  int peticion_size = sizeof(t_peticion_stdout);
  pthread_mutex_lock(&(hilo_out->io->socket_km->mutex_socket));
  bool envio = enviar_buffer(OP_PETICION_IO_STDOUT, peticion->peticion,
                             peticion_size, hilo_out->io->socket_km->socket_km);
  if (!envio)
  {
    logger_error(hilo_out->io->logger,
                 "## Error en la comunicacion con el Kernel Memory");
    return false;
    pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
  }
  pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
  return true;
}

bool envio_stdin(t_stdin* peticion, t_hilo_io_in* hilo_in, char* buffer)
{
  int peticion_size = sizeof(t_peticion_stdin);
  t_paquete* paquete = crear_paquete(OP_PETICION_IO_STDIN);
  agregar_string_a_paquete(paquete, buffer);
  agregar_a_paquete(paquete, peticion->peticion, peticion_size);
  pthread_mutex_lock(&(hilo_in->io->socket_km->mutex_socket));
  bool envio = enviar_paquete(paquete, hilo_in->io->socket_km->socket_km);
  if (!envio)
  {
    logger_error(hilo_in->io->logger, "## Error en el envio a Kernel memory");
    return false;
  }

  pthread_mutex_unlock(&(hilo_in->io->socket_km->mutex_socket));
  free(buffer);
  free(paquete);
  return true;
}
bool comunicacion_io_stdin(t_stdin* peticion, t_hilo_io_in* hilo_in,
                           char* buffer)
{
  int peticion_size = sizeof(t_peticion_stdin);
  bool envio = enviar_buffer(OP_PETICION_IO_STDIN, peticion->peticion,
                             peticion_size, hilo_in->io->socket_io);
  if (!envio)
  {
    logger_error(hilo_in->io->logger, "## Error em el envio a IO");

    return false;
  }
  logger_info(hilo_in->io->logger, "## (%d) - Solicitó syscall: STDIN",
              peticion->peticion->pid);

  // recibo la respuesta de IO
  int cod_op = recibir_operacion(hilo_in->io->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    return false;
  }
  buffer = recibir_string(hilo_in->io->socket_io);

  if (buffer == NULL)
  {
    logger_error(hilo_in->io->logger, "## Error al recibir la respuesa de IO");
    return false;
  }
  return true;
}

bool comunicacion_io_sleep(t_sleep* peticion, t_hilo_io_sleep* hilo_sleep)
{
  int peticion_size = sizeof(t_peticion_sleep);
  bool envio = enviar_buffer(OP_PETICION_IO_SLEEP, peticion->peticion,
                             peticion_size, hilo_sleep->io->socket_io);
  if (!envio)
  {
    logger_error(hilo_sleep->io->logger, "## Error al enviar a IO");
    return false;
  }

  int cod_op = recibir_operacion(hilo_sleep->io->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    logger_error(hilo_sleep->io->logger,
                 "## Error en la respuesta de IO a Kernel Scheduler");
    return false;
  }
  char* respuesta = recibir_string(hilo_sleep->io->socket_io);
  if (strcmp(respuesta, "OK") != 0)
  {
    logger_error(
        hilo_sleep->io->logger,
        "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    free(respuesta);
    return false;
  }
  free(respuesta);
  return true;
}
// funcion de finalizacion stdin (no se me ocurre un nombre mejor)
void finalizar_stdin(t_stdin* peticion, t_hilo_io_in* hilo_in)
{
  pthread_mutex_lock(&(hilo_in->lista_stdin->mutex_lista_stdin));
  if (list_remove_element(hilo_in->lista_stdin->lista_stdin, peticion) == 0)
  {
    pthread_mutex_unlock(&(hilo_in->lista_stdin->mutex_lista_stdin));
    logger_error(hilo_in->io->logger,
                 "## Error al retirar el proceso de la lista de IO");
    return;
  }
  pthread_mutex_unlock(&(hilo_in->lista_stdin->mutex_lista_stdin));
  logger_info(hilo_in->io->logger, "## PID %d - Retirado de la lista de IO",
              peticion->pcb->pid);

  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  cambio_desbloquear(peticion->pcb, hilo_in->io->colas);
  logger_info(hilo_in->io->logger,
              "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
              peticion->pcb->pid);
}
void finalizar_stdout(t_stdout* peticion, t_hilo_io_out* hilo_out)
{
  pthread_mutex_lock(&(hilo_out->lista_stdout->mutex_lista_stdout));
  if (list_remove_element(hilo_out->lista_stdout->lista_stdout, peticion) == 0)
  {
    pthread_mutex_unlock(&(hilo_out->lista_stdout->mutex_lista_stdout));
    logger_error(hilo_out->io->logger,
                 "## Error al retirar el proceso de la lista de IO");
    return;
  }
  pthread_mutex_unlock(&(hilo_out->lista_stdout->mutex_lista_stdout));
  logger_info(hilo_out->io->logger, "## PID %d - Retirado de la lista de IO",
              peticion->pcb->pid);
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  cambio_desbloquear(peticion->pcb, hilo_out->io->colas);
  logger_info(hilo_out->io->logger,
              "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
              peticion->pcb->pid);
}

void finalizar_sleep(t_hilo_io_sleep* hilo_sleep, t_sleep* peticion)
{
  pthread_mutex_lock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
  if (list_remove_element(hilo_sleep->lista_sleep->lista_sleep, peticion) == 0)
  {
    pthread_mutex_unlock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
    logger_error(hilo_sleep->io->logger,
                 "## Error al retirar el proceso de la lista de IO");
    return;
  }
  pthread_mutex_unlock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
  logger_info(hilo_sleep->io->logger, "## PID %d - Retirado de la lista de IO",
              peticion->pcb->pid);
  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  cambio_desbloquear(peticion->pcb, hilo_sleep->io->colas);
  logger_info(hilo_sleep->io->logger,
              "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
              peticion->pcb->pid);
}

int io_sleep_f(t_sleep* peticion, t_hilo_io_sleep* hilo_sleep)
{
  bool comms = comunicacion_io_sleep(peticion, hilo_sleep);
  if (!comms)
  {
    return D_ERROR_IO;
  }
  finalizar_sleep(hilo_sleep, peticion);
  return D_TODO_BIEN;
}

void cerrar_io_stdin(t_hilo_io_in* hilo_stdin)
{
  pthread_mutex_lock(&(hilo_stdin->io->mutex_socket_io));
  close(hilo_stdin->io->socket_io);
  hilo_stdin->io->socket_io = -1;
  pthread_mutex_unlock(&(hilo_stdin->io->mutex_socket_io));
  pthread_mutex_lock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
  while (!list_is_empty(hilo_stdin->lista_stdin->lista_stdin))
  {
    t_stdin* peticion = list_remove(hilo_stdin->lista_stdin->lista_stdin, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, hilo_stdin->io->colas);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
  pthread_mutex_destroy(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
  list_destroy(hilo_stdin->lista_stdin->lista_stdin);
  free(hilo_stdin->lista_stdin);
  pthread_mutex_unlock(&(hilo_stdin->io->mutex_fin));
  free(hilo_stdin);
}
bool charla_km_stdin(t_hilo_io_in* hilo_in)
{
  int cod_op = -1;
  cod_op = recibir_operacion(hilo_in->io->socket_io);

  switch (cod_op)
  {
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(hilo_in->io->socket_km->socket_km));
      cerrar_kernel_scheduler(hilo_in->io->socket_server, hilo_in->io->logger,
                              MC_MEMORIA_CORRUPTA);
      return false;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(hilo_in->io->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(hilo_in->io->colas);
      return charla_km_stdin(hilo_in);
    case OP_RESPUESTA_STDOUT:
      free(recibir_string(hilo_in->io->socket_km->socket_km));
      return true;
    default:
      free(recibir_string(hilo_in->io->socket_km->socket_km));
      cerrar_kernel_scheduler(hilo_in->io->socket_server, hilo_in->io->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
      return false;
  }
}
int io_stdin_f(t_stdin* peticion, t_hilo_io_in* hilo_in)
{
  // Envio peticion a IO
  char* buffer = malloc(sizeof(peticion->peticion->tamanio_a_leer));
  bool envio = comunicacion_io_stdin(peticion, hilo_in, buffer);
  if (!envio)
  {
    free(buffer);
    return false;
  }
  // Le envio el paquete al Kernel Memory para que escriba en la memoria
  envio = envio_stdin(peticion, hilo_in, buffer);
  if (!envio)
  {
    free(buffer);
    cerrar_kernel_scheduler(hilo_in->io->socket_server, hilo_in->io->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }
  pthread_mutex_lock(&(hilo_in->io->socket_km->mutex_socket));
  if (!(charla_km_stdin(hilo_in)))
  {
    pthread_mutex_unlock(&(hilo_in->io->socket_km->mutex_socket));
    return false;
  }
  pthread_mutex_unlock(&(hilo_in->io->socket_km->mutex_socket));
  finalizar_stdin(peticion, hilo_in);
  return true;
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

    t_stdin* peticion = list_get(hilo_stdin->lista_stdin->lista_stdin, 0);
    pthread_mutex_unlock(&(hilo_stdin->lista_stdin->mutex_lista_stdin));
    if (peticion == NULL)
    {
      logger_error(hilo_stdin->io->logger,
                   "## Error al obtener la peticion de la lista de stdin");
      seguir_atendiendo = false;
    }
    seguir_atendiendo = io_stdin_f(peticion, hilo_stdin);

    if (hilo_stdin->io->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  cerrar_io_stdin(hilo_stdin);
  return NULL;
}

// Funcion de cierre hilo stdout
void cerrar_hilo_stdout(t_hilo_io_out* hilo_stdout)
{
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
    cambio_desbloquear(pcb, hilo_stdout->io->colas);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
  pthread_mutex_destroy(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
  list_destroy(hilo_stdout->lista_stdout->lista_stdout);
  free(hilo_stdout->lista_stdout);
  pthread_mutex_unlock(&(hilo_stdout->io->mutex_fin));
  free(hilo_stdout);
}

bool recepcion_km_stdout(t_hilo_io_out* hilo_out)
{
  int op_code = -1;
  op_code = recibir_operacion(hilo_out->io->socket_km->socket_km);

  switch (op_code)
  {
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(hilo_out->io->socket_km->socket_km));
      cerrar_kernel_scheduler(hilo_out->io->socket_server, hilo_out->io->logger,
                              MC_MEMORIA_CORRUPTA);
      return false;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(hilo_out->io->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(hilo_out->io->colas);
      return recepcion_km_stdout(hilo_out);
    case OP_RESPUESTA_STDOUT:
      return true;
    default:
      free(recibir_string(hilo_out->io->socket_km->socket_km));
      cerrar_kernel_scheduler(hilo_out->io->socket_server, hilo_out->io->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
      return false;
  }
}

bool io_stdout_f(t_stdout* peticion, t_hilo_io_out* hilo_out)
{
  int cod_op = -1;
  // Envio peticion a Kernel Memory para que lea de la memoria
  bool envio = peticion_stdout_km(peticion, hilo_out);
  if (!envio)
  {
    cerrar_kernel_scheduler(hilo_out->io->socket_server, hilo_out->io->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }
  // Recibo la respuesta de Kernel Memory
  pthread_mutex_lock(&(hilo_out->io->socket_km->mutex_socket));

  if (!(recepcion_km_stdout(hilo_out)))
  {
    pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
    return false;
  }

  char* buffer =
      malloc(sizeof(char) * (peticion->peticion->tamanio_a_escribir + 1));
  buffer = recibir_string(hilo_out->io->socket_km->socket_km);
  pthread_mutex_unlock(&(hilo_out->io->socket_km->mutex_socket));
  if (buffer == NULL)
  {
    logger_error(hilo_out->io->logger,
                 "## Error al recibir la respuesa de Kernel Memory");
    free(buffer);
    cerrar_kernel_scheduler(hilo_out->io->socket_server, hilo_out->io->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }

  // Le envio el mensaje + la peticion a IO para que imprima por pantalla
  envio = envio_stdout(hilo_out, peticion, buffer);
  if (!envio)
  {
    free(buffer);
    return D_ERROR_IO;
  }
  free(buffer);
  pthread_mutex_lock(&(hilo_out->io->mutex_socket_io));
  cod_op = recibir_operacion(hilo_out->io->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    pthread_mutex_unlock(&(hilo_out->io->mutex_socket_io));
    logger_error(hilo_out->io->logger,
                 "## Error en la respuesta de IO a Kernel Scheduler");
    return false;
  }
  char* resp_io = recibir_string(hilo_out->io->socket_io);
  pthread_mutex_unlock(&(hilo_out->io->mutex_socket_io));
  free(resp_io);
  finalizar_stdout(peticion, hilo_out);
  return D_TODO_BIEN;
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
    pthread_mutex_lock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
    t_stdout* peticion = list_get(hilo_stdout->lista_stdout->lista_stdout, 0);
    pthread_mutex_unlock(&(hilo_stdout->lista_stdout->mutex_lista_stdout));
    if (peticion == NULL)
    {
      logger_error(hilo_stdout->io->logger,
                   "## Error al obtener la peticion de la lista de stdout");
      continue;
    }
    seguir_atendiendo = io_stdout_f(peticion, hilo_stdout);

    if (hilo_stdout->io->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  cerrar_hilo_stdout(hilo_stdout);
  return NULL;
}

// Funcion para cerrar el hilo sleep
void cerrar_hilo_sleep(t_hilo_io_sleep* hilo_sleep)
{
  pthread_mutex_lock(&(hilo_sleep->io->mutex_socket_io));
  close(hilo_sleep->io->socket_io);
  hilo_sleep->io->socket_io = -1;
  pthread_mutex_unlock(&(hilo_sleep->io->mutex_socket_io));

  pthread_mutex_lock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
  while (!(list_is_empty(hilo_sleep->lista_sleep->lista_sleep)))
  {
    t_sleep* peticion = list_remove(hilo_sleep->lista_sleep->lista_sleep, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, hilo_sleep->io->colas);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
  pthread_mutex_destroy(&(hilo_sleep->lista_sleep->mutex_lista_sleep));
  list_destroy(hilo_sleep->lista_sleep->lista_sleep);
  free(hilo_sleep->lista_sleep);
  pthread_mutex_unlock(&(hilo_sleep->io->mutex_fin));
  free(hilo_sleep);
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
    pthread_mutex_lock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
    t_sleep* peticion = list_get(shilo_sleep->lista_sleep->lista_sleep, 0);
    pthread_mutex_unlock(&(shilo_sleep->lista_sleep->mutex_lista_sleep));
    if (peticion == NULL)
    {
      logger_error(shilo_sleep->io->logger,
                   "## Error al obtener la peticion de la lista de sleep");
      continue;
    }
    if (D_ERROR_IO == io_sleep_f(peticion, shilo_sleep))
    {
      logger_error(shilo_sleep->io->logger,
                   "## Error al atender la petición de sleep");
      seguir_atendiendo = false;
    }
    if (shilo_sleep->io->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
  }
  cerrar_hilo_sleep(hilo_sleep);
  return NULL;
}

int obtener_tipo_io(int socket_fd, t_logger* logger)
{
  if (recibir_operacion(socket_fd) != OP_TIPO_IO)
  {
    logger_error(logger,
                 "## Error en el tipo de operación. Expected: OP_TIPO_IO");
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
    logger_error(logger, "## Tipo de IO no válido: %s", buffer);
    free(buffer);
    return -1;
  }
  logger_info(logger, "## IO de tipo %s conectada", buffer);
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

bool atender_nuevo_io(t_io io[3], int socket_fd, t_colas* colas,
                      t_listas_io* listas_io, bool prioridad_activa)
{
  if (!responder_handshake(socket_fd, MID_KERNEL_SCHEDULER, colas->logger))
    return false;

  int tipo_io = obtener_tipo_io(socket_fd, colas->logger);
  if (tipo_io == -1)
    return false;

  if (io[tipo_io].socket_io != -1)
  {
    logger_error(colas->logger, "## IO de tipo repetido: %d. Cerrando conexión",
                 tipo_io);
    close(socket_fd);
    return false;
  }
  // Preparo el t_io para crear el hilo
  io[tipo_io].socket_io = socket_fd;
  io[tipo_io].proceso_actual = NULL;
  pthread_mutex_init(&(io[tipo_io].mutex_fin), NULL);
  pthread_cond_init(&(io[tipo_io].nuevo_proceso), NULL);
  io[tipo_io].colas = colas;
  io[tipo_io].logger = colas->logger;
  io[tipo_io].socket_km = colas->socket_km;
  io[tipo_io].prioridad_activa = prioridad_activa;

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
        logger_error(colas->logger,
                     "## Error al crear el hilo para IO de tipo stdin");
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
        logger_error(colas->logger,
                     "## Error al crear el hilo para IO de tipo stdout");
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
        logger_error(colas->logger,
                     "## Error al crear el hilo para IO de tipo sleep");
        return false;
      }
      break;
  }

  return true;
}

bool comparar_prioridad_stdin(void* syscall1, void* syscall2)
{
  t_stdin* stdin1 = (t_stdin*)syscall1;
  t_stdin* stdin2 = (t_stdin*)syscall2;
  if (stdin1->pcb->prioridad > stdin2->pcb->prioridad)
  {
    return 1;
  }
  else if (stdin1->pcb->prioridad < stdin2->pcb->prioridad)
  {
    return 0;
  }
  else
  {
    return 0;
  }
}
bool procesar_nuevo_stdin(t_peticion_stdin* peticion, t_io* io_stdin,
                          t_lista_stdin* lista_stdin, t_pcb* pcb)
{
  pthread_mutex_lock(&(io_stdin->mutex_socket_io));
  if (io_stdin->socket_io == -1)
  {
    pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
    return false;
  }
  t_stdin* stdin = malloc(sizeof(t_stdin));
  stdin->peticion = peticion;
  stdin->pcb = pcb;

  if (io_stdin->prioridad_activa)
  {
    pthread_mutex_lock(&(lista_stdin->mutex_lista_stdin));
    bool lista_vacia = list_is_empty(lista_stdin->lista_stdin);
    list_add_sorted(lista_stdin->lista_stdin, stdin, comparar_prioridad_stdin);
    if (lista_vacia)
    {
      pthread_cond_signal(&(io_stdin->nuevo_proceso));
    }
    pthread_mutex_unlock(&(lista_stdin->mutex_lista_stdin));
    pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
    return true;
  }

  pthread_mutex_lock(&(lista_stdin->mutex_lista_stdin));
  bool lista_vacia = list_is_empty(lista_stdin->lista_stdin);
  list_add(lista_stdin->lista_stdin, stdin);
  if (lista_vacia)
  {
    pthread_cond_signal(&(io_stdin->nuevo_proceso));
  }
  pthread_mutex_unlock(&(lista_stdin->mutex_lista_stdin));
  pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
  return true;
}

bool comparar_prioridad_stdout(void* syscall1, void* syscall2)
{
  t_stdout* stdout1 = (t_stdout*)syscall1;
  t_stdout* stdout2 = (t_stdout*)syscall2;
  if (stdout1->pcb->prioridad > stdout2->pcb->prioridad)
  {
    return 1;
  }
  else if (stdout1->pcb->prioridad < stdout2->pcb->prioridad)
  {
    return 0;
  }
  else
  {
    return 0;
  }
}

bool procesar_nuevo_stdout(t_peticion_stdout* peticion, t_io* io_stdout,
                           t_lista_stdout* lista_stdout, t_pcb* pcb)
{
  pthread_mutex_lock(&(io_stdout->mutex_socket_io));
  if (io_stdout->socket_io == -1)
  {
    pthread_mutex_unlock(&(io_stdout->mutex_socket_io));
    return false;
  }
  t_stdout* stdout = malloc(sizeof(t_stdout));
  stdout->peticion = peticion;
  stdout->pcb = pcb;

  if (io_stdout->prioridad_activa)
  {
    pthread_mutex_lock(&(lista_stdout->mutex_lista_stdout));
    bool lista_vacia = list_is_empty(lista_stdout->lista_stdout);
    list_add_sorted(lista_stdout->lista_stdout, stdout,
                    comparar_prioridad_stdout);
    if (lista_vacia)
    {
      pthread_cond_signal(&(io_stdout->nuevo_proceso));
    }
    pthread_mutex_unlock(&(lista_stdout->mutex_lista_stdout));
    pthread_mutex_unlock(&(io_stdout->mutex_socket_io));
    return true;
  }
  pthread_mutex_lock(&(lista_stdout->mutex_lista_stdout));
  bool lista_vacia = list_is_empty(lista_stdout->lista_stdout);
  list_add(lista_stdout->lista_stdout, stdout);

  if (lista_vacia)
  {
    pthread_cond_signal(&(io_stdout->nuevo_proceso));
  }
  pthread_mutex_unlock(&(lista_stdout->mutex_lista_stdout));
  pthread_mutex_unlock(&(io_stdout->mutex_socket_io));
  return true;
}
bool comparar_prioridad_sleep(void* syscall1, void* syscall2)
{
  t_sleep* sleep1 = (t_sleep*)syscall1;
  t_sleep* sleep2 = (t_sleep*)syscall2;
  if (sleep1->pcb->prioridad > sleep2->pcb->prioridad)
  {
    return 1;
  }
  else if (sleep1->pcb->prioridad < sleep2->pcb->prioridad)
  {
    return 0;
  }
  else
  {
    return 0;
  }
}

bool procesar_nuevo_sleep(t_peticion_sleep* peticion, t_io* io_sleep,
                          t_lista_sleep* lista_sleep, t_pcb* pcb)
{
  pthread_mutex_lock(&(io_sleep->mutex_socket_io));
  if (io_sleep->socket_io == -1)
  {
    pthread_mutex_unlock(&(io_sleep->mutex_socket_io));
    return false;
  }
  t_sleep* sleep = malloc(sizeof(t_sleep));
  sleep->peticion = peticion;
  sleep->pcb = pcb;

  if (io_sleep->prioridad_activa)
  {
    pthread_mutex_lock(&(lista_sleep->mutex_lista_sleep));
    bool lista_vacia = list_is_empty(lista_sleep->lista_sleep);
    list_add_sorted(lista_sleep->lista_sleep, sleep, comparar_prioridad_sleep);
    if (lista_vacia)
    {
      pthread_cond_signal(&(io_sleep->nuevo_proceso));
    }
    pthread_mutex_unlock(&(lista_sleep->mutex_lista_sleep));
    pthread_mutex_unlock(&(io_sleep->mutex_socket_io));
    return true;
  }

  pthread_mutex_lock(&(lista_sleep->mutex_lista_sleep));
  if (list_is_empty(lista_sleep->lista_sleep))
  {
    pthread_cond_signal(&(io_sleep->nuevo_proceso));
  }
  list_add(lista_sleep->lista_sleep, sleep);
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

void cerrar_io(t_io* io, t_listas_io* listas_io)
{
  for (int i = 0; i < 3; i++)
  {
    io[i].cerrar_hilo = true;
    pthread_mutex_lock(&(io[i].mutex_fin));
    destruir_io(&io[i]);
  }
  free(listas_io);
  free(io);
}

/*

io[i].cerrar_hilo no está protegido con mutex

Los siguientes 3 structs son exactamente iguales:

typedef struct
{
  t_list* lista_stdin;
  pthread_mutex_t mutex_lista_stdin;
} t_lista_stdin;

typedef struct
{
  t_list* lista_stdout;
  pthread_mutex_t mutex_lista_stdout;
} t_lista_stdout;

typedef struct
{
  t_list* lista_sleep;
  pthread_mutex_t mutex_lista_sleep;
} t_lista_sleep;

Los siguientes 3 structs son exactamente iguales:

typedef struct
{
  t_io* io;
  t_lista_stdin* lista_stdin;
} t_hilo_io_in;

typedef struct
{
  t_io* io;
  t_lista_stdout* lista_stdout;
} t_hilo_io_out;

typedef struct
{
  t_io* io;
  t_lista_sleep* lista_sleep;
} t_hilo_io_sleep;

La estructura de abajo tiene 3 elementos iguales, reemplazar por un arreglo o
que sea parte de t_io (creo que es lo mejor) Podes acceder a las posiciones del
arreglo con la estructura de tipo de io que está en la utils

typedef struct
{
  t_lista_stdin* lista_stdin;
  t_lista_stdout* lista_stdout;
  t_lista_sleep* lista_sleep;
} t_listas_io;

El pthread_mutex_lock(&(io[i].mutex_fin)); no tiene sentido

Podes usar un pthread_join(io[i].hilo_io); (es mucho mejor que tener un mutex
específico para eso)

Cuando unifiques las estructuras de arriba, probáblemente puedas eliminar alguna
funcion que tenés triplicada

Las listas son un t_list* y guardan los elementos como void*, el tipo de dato es
el mismo para cualquier cosa que pueda llegar a contener la lista, todos son
t_list*, por eso podés unificar las structus de arriba

Cuando hay que cerrar los IOs, tal vez tengas que hacer un shutdown del socket
de io. Si justo se está ejecutando un delay de 25 segundo y hay una BSOD (memory
stick desconectado), con el shutdown podés hacer que todos los que estén
esperando respuesta por ese socket se desbloqueen y les de op_code de error

Cuando cerras los io, que pasa si un hilo ya cerró antes o nunca se abrió y se
ejecuta esa función
*/
