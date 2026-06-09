
#include "kernel_scheduler/io.h"

#include <string.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/io.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/msg.h"
#include "utils/registros.h"

void inicializar_listas_io(t_io io[3])
{
   for (int i = 0; i < 3; i++)
  {
    io[i].lista_io = malloc(sizeof(t_lista_io));
    io[i].lista_io->lista_io = list_create();

  }
}

// Funciones de comunicacion de syscalls IO
bool envio_stdout(t_io* io_out, t_stdout* peticion, char* buffer)
{
  int peticion_size = sizeof(t_peticion_stdout);
  t_paquete* paquete = crear_paquete(OP_PETICION_IO_STDOUT);
  agregar_a_paquete(paquete, peticion->peticion, peticion_size);
  agregar_string_a_paquete(paquete, buffer);
  pthread_mutex_lock(&(io_out->mutex_socket_io));
  bool envio = enviar_paquete(paquete, io_out->socket_io);
  pthread_mutex_unlock(&(io_out->mutex_socket_io));
  eliminar_paquete(paquete);
  free(buffer);
  if (!envio)
  {
    logger_error(io_out->logger,
                 "## Error al enviar la respuesta de Kernel Memory a IO");
    return false;
  }
  int cod_op = recibir_operacion(io_out->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    return false;
  }
  return true;
}

bool peticion_stdout_km(t_stdout* peticion, t_io* io_out)
{
  int peticion_size = sizeof(t_peticion_stdout);
  pthread_mutex_lock(&(io_out->socket_km->mutex_socket));
  bool envio = enviar_buffer(OP_PETICION_IO_STDOUT, peticion->peticion,
                             peticion_size, io_out->socket_km->socket_km);
  if (!envio)
  {
    logger_error(io_out->logger,
                 "## Error en la comunicacion con el Kernel Memory");
    return false;
    pthread_mutex_unlock(&(io_out->socket_km->mutex_socket));
  }
  pthread_mutex_unlock(&(io_out->socket_km->mutex_socket));
  return true;
}

bool envio_stdin(t_stdin* peticion, t_io* io_in, char* buffer)
{
  int peticion_size = sizeof(t_peticion_stdin);
  t_paquete* paquete = crear_paquete(OP_PETICION_IO_STDIN);
  agregar_string_a_paquete(paquete, buffer);
  agregar_a_paquete(paquete, peticion->peticion, peticion_size);
  pthread_mutex_lock(&(io_in->socket_km->mutex_socket));
  bool envio = enviar_paquete(paquete, io_in->socket_km->socket_km);
  if (!envio)
  {
    logger_error(io_in->logger, "## Error en el envio a Kernel memory");
    return false;
  }

  pthread_mutex_unlock(&(io_in->socket_km->mutex_socket));
  free(buffer);
  free(paquete);
  return true;
}
bool comunicacion_io_stdin(t_stdin* peticion, t_io* io_in,
                           char* buffer)
{
  int peticion_size = sizeof(t_peticion_stdin);
  bool envio = enviar_buffer(OP_PETICION_IO_STDIN, peticion->peticion,
                             peticion_size, io_in->socket_io);
  if (!envio)
  {
    logger_error(io_in->logger, "## Error em el envio a IO");

    return false;
  }
  logger_info(io_in->logger, "## (%d) - Solicitó syscall: STDIN",
              peticion->peticion->pid);

  // recibo la respuesta de IO
  int cod_op = recibir_operacion(io_in->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    return false;
  }
  buffer = recibir_string(io_in->socket_io);

  if (buffer == NULL)
  {
    logger_error(io_in->logger, "## Error al recibir la respuesa de IO");
    return false;
  }
  return true;
}

bool comunicacion_io_sleep(t_sleep* peticion, t_io* io_sleep)
{
  int peticion_size = sizeof(t_peticion_sleep);
  bool envio = enviar_buffer(OP_PETICION_IO_SLEEP, peticion->peticion,
                             peticion_size, io_sleep->socket_io);
  if (!envio)
  {
    logger_error(io_sleep->logger, "## Error al enviar a IO");
    return false;
  }

  int cod_op = recibir_operacion(io_sleep->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    logger_error(io_sleep->logger,
                 "## Error en la respuesta de IO a Kernel Scheduler");
    return false;
  }
  char* respuesta = recibir_string(io_sleep->socket_io);
  if (strcmp(respuesta, "OK") != 0)
  {
    logger_error(
        io_sleep->logger,
        "## Error en la respuesta de IO a Kernel Scheduler. Expected: OK");
    free(respuesta);
    return false;
  }
  free(respuesta);
  return true;
}
// funcion de finalizacion io (no se me ocurre un nombre mejor)
void finalizar_stdin(t_stdin* peticion, t_io* io)
{
  pthread_mutex_lock(&(io->lista_io->mutex_lista_io));
  if (list_remove_element(io->lista_io->lista_io, peticion) == 0)
  {
    pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
    logger_error(io->logger,
                 "## Error al retirar el proceso de la lista de IO");
    return;
  }
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  logger_info(io->logger, "## PID %d - Retirado de la lista de IO",
              peticion->pcb->pid);

  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  cambio_desbloquear(peticion->pcb, io->colas);
  logger_info(io->logger,
              "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
              peticion->pcb->pid);
}

void finalizar_stdout(t_stdout* peticion, t_io* io)
{
  pthread_mutex_lock(&(io->lista_io->mutex_lista_io));
  if (list_remove_element(io->lista_io->lista_io, peticion) == 0)
  {
    pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
    logger_error(io->logger,
                 "## Error al retirar el proceso de la lista de IO");
    return;
  }
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  logger_info(io->logger, "## PID %d - Retirado de la lista de IO",
              peticion->pcb->pid);

  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  cambio_desbloquear(peticion->pcb, io->colas);
  logger_info(io->logger,
              "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
              peticion->pcb->pid);
}

void finalizar_sleep(t_sleep* peticion, t_io* io)
{
  pthread_mutex_lock(&(io->lista_io->mutex_lista_io));
  if (list_remove_element(io->lista_io->lista_io, peticion) == 0)
  {
    pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
    logger_error(io->logger,
                 "## Error al retirar el proceso de la lista de IO");
    return;
  }
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  logger_info(io->logger, "## PID %d - Retirado de la lista de IO",
              peticion->pcb->pid);

  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  cambio_desbloquear(peticion->pcb, io->colas);
  logger_info(io->logger,
              "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
              peticion->pcb->pid);
}



int io_sleep_f(t_sleep* peticion, t_io* io_sleep)
{
  bool comms = comunicacion_io_sleep(peticion, io_sleep);
  if (!comms)
  {
    return D_ERROR_IO;
  }
  finalizar_sleep(peticion, io_sleep);
  return D_TODO_BIEN;
}

void cerrar_io_stdin(t_io* io_stdin)
{
  pthread_mutex_lock(&(io_stdin->mutex_socket_io));
  close(io_stdin->socket_io);
  io_stdin->socket_io = -1;
  pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
  pthread_mutex_lock(&(io_stdin->lista_io->mutex_lista_io));
  while (!list_is_empty(io_stdin->lista_io->lista_io))
  {
    t_stdin* peticion = malloc(sizeof(t_stdin));
    peticion = list_remove(io_stdin->lista_io->lista_io, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, io_stdin->colas);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(io_stdin->lista_io->mutex_lista_io));
  pthread_mutex_destroy(&(io_stdin->lista_io->mutex_lista_io));
  list_destroy(io_stdin->lista_io->lista_io);
  free(io_stdin->lista_io);
}
bool charla_km_stdin(t_io* io_in)
{
  int cod_op = -1;
  cod_op = recibir_operacion(io_in->socket_io);

  switch (cod_op)
  {
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(io_in->socket_km->socket_km));
      cerrar_kernel_scheduler(io_in->socket_server, io_in->logger,
                              MC_MEMORIA_CORRUPTA);
      return false;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(io_in->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(io_in->colas);
      return charla_km_stdin(io_in);
    case OP_RESPUESTA_STDOUT:
      free(recibir_string(io_in->socket_km->socket_km));
      return true;
    default:
      free(recibir_string(io_in->socket_km->socket_km));
      cerrar_kernel_scheduler(io_in->socket_server, io_in->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
      return false;
  }
}
int io_stdin_f(t_stdin* peticion, t_io* io_in)
{
  // Envio peticion a IO
  char* buffer = malloc(sizeof(peticion->peticion->tamanio_a_leer));
  bool envio = comunicacion_io_stdin(peticion, io_in, buffer);
  if (!envio)
  {
    free(buffer);
    return false;
  }
  // Le envio el paquete al Kernel Memory para que escriba en la memoria
  envio = envio_stdin(peticion, io_in, buffer);
  if (!envio)
  {
    free(buffer);
    cerrar_kernel_scheduler(io_in->socket_server, io_in->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }
  pthread_mutex_lock(&(io_in->socket_km->mutex_socket));
  if (!(charla_km_stdin(io_in)))
  {
    pthread_mutex_unlock(&(io_in->socket_km->mutex_socket));
    return false;
  }
  pthread_mutex_unlock(&(io_in->socket_km->mutex_socket));
  finalizar_stdin(peticion, io_in);
  return true;
}
void* hilo_io_in(void* hilo_in)
{
  t_io* io_stdin = (t_io*)hilo_in;
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(io_stdin->lista_io->mutex_lista_io));
    while (list_is_empty(io_stdin->lista_io->lista_io))
    {
      pthread_cond_wait(&(io_stdin->nuevo_proceso),
                        &(io_stdin->lista_io->mutex_lista_io));
    }
    pthread_mutex_lock(&(io_stdin->lista_io->mutex_lista_io));
    t_stdin* peticion = list_get(io_stdin->lista_io->lista_io, 0);
    pthread_mutex_unlock(&(io_stdin->lista_io->mutex_lista_io));
    if (peticion == NULL)
    {
      logger_error(io_stdin->logger,
                   "## Error al obtener la peticion de la lista de stdin");
      seguir_atendiendo = false;
    }
    seguir_atendiendo = io_stdin_f(peticion, io_stdin);
    pthread_mutex_lock(&(io_stdin->mutex_fin));
    if (io_stdin->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
     pthread_mutex_unlock(&(io_stdin->mutex_fin));
  }
  cerrar_io_stdin(io_stdin);
  return NULL;
}

// Funcion de cierre hilo stdout
void cerrar_hilo_stdout(t_io* io_stdout)
{
  pthread_mutex_lock(&(io_stdout->mutex_socket_io));
  close(io_stdout->socket_io);
  io_stdout->socket_io = -1;
  pthread_mutex_unlock(&(io_stdout->mutex_socket_io));

  // cierro la lista de stdout
  pthread_mutex_lock(&(io_stdout->lista_io->mutex_lista_io));
  while (!list_is_empty(io_stdout->lista_io->lista_io))
  {
    t_stdout* peticion = malloc(sizeof(t_stdout));
    peticion = list_remove(io_stdout->lista_io->lista_io, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, io_stdout->colas);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(io_stdout->lista_io->mutex_lista_io));
  pthread_mutex_destroy(&(io_stdout->lista_io->mutex_lista_io));
  list_destroy(io_stdout->lista_io->lista_io);
  free(io_stdout->lista_io);
}

bool recepcion_km_stdout(t_io* io_out)
{
  int op_code = -1;
  op_code = recibir_operacion(io_out->socket_km->socket_km);

  switch (op_code)
  {
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(io_out->socket_km->socket_km));
      cerrar_kernel_scheduler(io_out->socket_server, io_out->logger,
                              MC_MEMORIA_CORRUPTA);
      return false;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(io_out->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(io_out->colas);
      return recepcion_km_stdout(io_out);
    case OP_RESPUESTA_STDOUT:
      free(recibir_string(io_out->socket_km->socket_km));
      return true;
    default:
      free(recibir_string(io_out->socket_km->socket_km));
      cerrar_kernel_scheduler(io_out->socket_server, io_out->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
      return false;
  }
}

bool io_stdout_f(t_stdout* peticion, t_io* io_out)
{
  int cod_op = -1;
  // Envio peticion a Kernel Memory para que lea de la memoria
  bool envio = peticion_stdout_km(peticion, io_out);
  if (!envio)
  {
    cerrar_kernel_scheduler(io_out->socket_server, io_out->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }
  // Recibo la respuesta de Kernel Memory
  pthread_mutex_lock(&(io_out->socket_km->mutex_socket));

  if (!(recepcion_km_stdout(io_out)))
  {
    pthread_mutex_unlock(&(io_out->socket_km->mutex_socket));
    return false;
  }

  char* buffer =
      malloc(sizeof(char) * (peticion->peticion->tamanio_a_escribir));
  buffer = recibir_string(io_out->socket_km->socket_km);
  pthread_mutex_unlock(&(io_out->socket_km->mutex_socket));
  if (buffer == NULL)
  {
    logger_error(io_out->logger,
                 "## Error al recibir la respuesa de Kernel Memory");
    free(buffer);
    cerrar_kernel_scheduler(io_out->socket_server, io_out->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }

  // Le envio el mensaje + la peticion a IO para que imprima por pantalla
  envio = envio_stdout(io_out, peticion, buffer);
  if (!envio)
  {
    free(buffer);
    return D_ERROR_IO;
  }
  free(buffer);
  pthread_mutex_lock(&(io_out->mutex_socket_io));
  cod_op = recibir_operacion(io_out->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    pthread_mutex_unlock(&(io_out->mutex_socket_io));
    logger_error(io_out->logger,
                 "## Error en la respuesta de IO a Kernel Scheduler");
    return false;
  }
  char* resp_io = recibir_string(io_out->socket_io);
  pthread_mutex_unlock(&(io_out->mutex_socket_io));
  free(resp_io);
  finalizar_stdout(peticion, io_out);
  return true;
}

void* hilo_io_out(void* hilo_out)
{
  t_io* io_stdout = (t_io*)hilo_out;

  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(io_stdout->lista_io->mutex_lista_io));
    while (list_is_empty(io_stdout->lista_io->lista_io))
    {
      pthread_cond_wait(&(io_stdout->nuevo_proceso),
                        &(io_stdout->lista_io->mutex_lista_io));
    }
    pthread_mutex_lock(&(io_stdout->lista_io->mutex_lista_io));
    t_stdout* peticion = list_get(io_stdout->lista_io->lista_io, 0);
    pthread_mutex_unlock(&(io_stdout->lista_io->mutex_lista_io));
    if (peticion == NULL)
    {
      logger_error(io_stdout->logger,
                   "## Error al obtener la peticion de la lista de stdout");
      continue;
    }
    seguir_atendiendo = io_stdout_f(peticion, io_stdout);
    pthread_mutex_lock(&(io_stdout->mutex_fin));
    if (io_stdout->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
    pthread_mutex_unlock(&(io_stdout->mutex_fin));
  }
  cerrar_hilo_stdout(io_stdout);
  return NULL;
}

// Funcion para cerrar el hilo sleep
void cerrar_hilo_sleep(t_io* io_sleep)
{
  pthread_mutex_lock(&(io_sleep->mutex_socket_io));
  close(io_sleep->socket_io);
  io_sleep->socket_io = -1;
  pthread_mutex_unlock(&(io_sleep->mutex_socket_io));

  pthread_mutex_lock(&(io_sleep->lista_io->mutex_lista_io));
  while (!(list_is_empty(io_sleep->lista_io->lista_io)))
  {
    t_sleep* peticion = list_remove(io_sleep->lista_io->lista_io, 0);
    t_pcb* pcb = peticion->pcb;
    cambio_desbloquear(pcb, io_sleep->colas);
    free(peticion->peticion);
    free(peticion);
  }
  pthread_mutex_unlock(&(io_sleep->lista_io->mutex_lista_io));
  pthread_mutex_destroy(&(io_sleep->lista_io->mutex_lista_io));
  list_destroy(io_sleep->lista_io->lista_io);
  free(io_sleep->lista_io);
}

void* hilo_io_sleep(void* hilo_sleep)
{
  t_io* io_sleep = (t_io*)hilo_sleep;
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(io_sleep->lista_io->mutex_lista_io));
    while (list_is_empty(io_sleep->lista_io->lista_io))
    {
      pthread_cond_wait(&(io_sleep->nuevo_proceso),
                        &(io_sleep->lista_io->mutex_lista_io));
    }
    pthread_mutex_lock(&(io_sleep->lista_io->mutex_lista_io));
    t_sleep* peticion = list_get(io_sleep->lista_io->lista_io, 0);
    pthread_mutex_unlock(&(io_sleep->lista_io->mutex_lista_io));
    if (peticion == NULL)
    {
      logger_error(io_sleep->logger,
                   "## Error al obtener la peticion de la lista de sleep");
      continue;
    }
    if (D_ERROR_IO == io_sleep_f(peticion, io_sleep))
    {
      logger_error(io_sleep->logger,
                   "## Error al atender la petición de sleep");
      seguir_atendiendo = false;
    }
    pthread_mutex_lock(&(io_sleep->mutex_fin));
    if (io_sleep->cerrar_hilo)
    {
      seguir_atendiendo = false;
    }
    pthread_mutex_unlock(&(io_sleep->mutex_fin));
  }
  cerrar_hilo_sleep(io_sleep);
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


bool atender_nuevo_io(t_io io[3], int socket_fd, t_colas* colas, bool prioridad_activa)
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
      pthread_mutex_init(&(io[tipo_io].lista_io->mutex_lista_io), NULL);
      if (pthread_create(&(io[tipo_io].hilo_io), NULL, hilo_io_in,
                         (void*)(&(io[tipo_io]))))
      {
        logger_error(colas->logger,
                     "## Error al crear el hilo para IO de tipo stdin");
        return false;
      }
      break;
    case E_STDOUT:
      pthread_mutex_init(&(io[tipo_io].lista_io->mutex_lista_io),
                         NULL);
      if (pthread_create(&(io[tipo_io].hilo_io), NULL, hilo_io_out,
                         (void*)(&(io[tipo_io]))))
      {
        logger_error(colas->logger,
                     "## Error al crear el hilo para IO de tipo stdout");
        return false;
      }
      break;
    case E_SLEEP:
      pthread_mutex_init(&(io[tipo_io].lista_io->mutex_lista_io), NULL);
      if (pthread_create(&(io[tipo_io].hilo_io), NULL, hilo_io_sleep,
                         (void*)(&(io[tipo_io]))))
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
  return (stdin1->pcb->prioridad <= stdin2->pcb->prioridad);
}
bool procesar_nuevo_stdin(t_peticion_stdin* peticion, t_io* io_stdin, t_pcb* pcb)
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
    pthread_mutex_lock(&(io_stdin->lista_io->mutex_lista_io));
    bool lista_vacia = list_is_empty(io_stdin->lista_io->lista_io);
    list_add_sorted(io_stdin->lista_io->lista_io, stdin, comparar_prioridad_stdin);
    if (lista_vacia)
    {
      pthread_cond_signal(&(io_stdin->nuevo_proceso));
    }
    pthread_mutex_unlock(&(io_stdin->lista_io->mutex_lista_io));
    pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
    return true;
  }

  pthread_mutex_lock(&(io_stdin->lista_io->mutex_lista_io));
  bool lista_vacia = list_is_empty(io_stdin->lista_io->lista_io);
  list_add(io_stdin->lista_io->lista_io, stdin);
  if (lista_vacia)
  {
    pthread_cond_signal(&(io_stdin->nuevo_proceso));
  }
  pthread_mutex_unlock(&(io_stdin->lista_io->mutex_lista_io));
  pthread_mutex_unlock(&(io_stdin->mutex_socket_io));
  return true;
}

bool comparar_prioridad_stdout(void* syscall1, void* syscall2)
{
  t_stdout* stdout1 = (t_stdout*)syscall1;
  t_stdout* stdout2 = (t_stdout*)syscall2;
  return (stdout1->pcb->prioridad <= stdout2->pcb->prioridad);
}

bool procesar_nuevo_stdout(t_peticion_stdout* peticion, t_io* io_stdout, t_pcb* pcb)
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
    pthread_mutex_lock(&(io_stdout->lista_io->mutex_lista_io));
    bool lista_vacia = list_is_empty(io_stdout->lista_io->lista_io);
    list_add_sorted(io_stdout->lista_io->lista_io, stdout,
                    comparar_prioridad_stdout);
    if (lista_vacia)
    {
      pthread_cond_signal(&(io_stdout->nuevo_proceso));
    }
    pthread_mutex_unlock(&(io_stdout->lista_io->mutex_lista_io));
    pthread_mutex_unlock(&(io_stdout->mutex_socket_io));
    return true;
  }
  pthread_mutex_lock(&(io_stdout->lista_io->mutex_lista_io));
  bool lista_vacia = list_is_empty(io_stdout->lista_io->lista_io);
  list_add(io_stdout->lista_io->lista_io, stdout);

  if (lista_vacia)
  {
    pthread_cond_signal(&(io_stdout->nuevo_proceso));
  }
  pthread_mutex_unlock(&(io_stdout->lista_io->mutex_lista_io));
  pthread_mutex_unlock(&(io_stdout->mutex_socket_io));
  return true;
}
bool comparar_prioridad_sleep(void* syscall1, void* syscall2)
{
  t_sleep* sleep1 = (t_sleep*)syscall1;
  t_sleep* sleep2 = (t_sleep*)syscall2;
  return (sleep1->pcb->prioridad <= sleep2->pcb->prioridad);
}

bool procesar_nuevo_sleep(t_peticion_sleep* peticion, t_io* io_sleep, t_pcb* pcb)
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
    pthread_mutex_lock(&(io_sleep->lista_io->mutex_lista_io));
    bool lista_vacia = list_is_empty(io_sleep->lista_io->lista_io);
    list_add_sorted(io_sleep->lista_io->lista_io, sleep, comparar_prioridad_sleep);
    if (lista_vacia)
    {
      pthread_cond_signal(&(io_sleep->nuevo_proceso));
    }
    pthread_mutex_unlock(&(io_sleep->lista_io->mutex_lista_io));
    pthread_mutex_unlock(&(io_sleep->mutex_socket_io));
    return true;
  }

  pthread_mutex_lock(&(io_sleep->lista_io->mutex_lista_io));
  if (list_is_empty(io_sleep->lista_io->lista_io))
  {
    pthread_cond_signal(&(io_sleep->nuevo_proceso));
  }
  list_add(io_sleep->lista_io->lista_io, sleep);
  pthread_mutex_unlock(&(io_sleep->lista_io->mutex_lista_io));
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

void cerrar_io(t_io io[3])
{
  for (int i = 0; i < 3; i++)
  {
    if(io[i].socket_io != -1){
    pthread_mutex_lock(&(io{i}->mutex_fin));
    io[i].cerrar_hilo = true;
    pthread_mutex_lock(&(io{i}->mutex_fin));
    pthread_join(io[i].hilo_io);
    destruir_io(&io[i]);
  }
}
  free(io);
}
