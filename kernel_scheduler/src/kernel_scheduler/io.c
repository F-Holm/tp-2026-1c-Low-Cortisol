
#include "kernel_scheduler/io.h"

#include <string.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/io.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/msg.h"
#include "utils/registros.h"


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
  bool envio = enviar_buffer(OP_PETICION_IO_STDOUT, peticion->peticion,
                             peticion_size, io_out->socket_km->socket_km);
  if (!envio)
  {
    logger_error(io_out->logger,
                 "## Error en la comunicacion con el Kernel Memory");
    return false;
  }
  return true;
}

bool envio_stdin(t_stdin* peticion, t_io* io_in, char* buffer)
{
  int peticion_size = sizeof(t_peticion_stdin);
  t_paquete* paquete = crear_paquete(OP_PETICION_IO_STDIN);
  agregar_string_a_paquete(paquete, buffer);
  agregar_a_paquete(paquete, peticion->peticion, peticion_size);
  bool envio = enviar_paquete(paquete, io_in->socket_km->socket_km);
  eliminar_paquete(paquete);
  if (!envio)
  {
    logger_error(io_in->logger, "## Error en el envio a Kernel memory");
    return false;
  }
  free(buffer);
  return true;
}
bool comunicacion_io_stdin(t_stdin* peticion, t_io* io_in, char* buffer)
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
void finalizar_io(void* peticion, t_io* io, t_pcb* pcb)
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
  logger_info(io->logger, "## PID %d - Retirado de la lista de IO", pcb->pid);

  // Paso a ready o susp ready dependiendo del tiempo bloqueado
  cambio_desbloquear(pcb, io->colas);
  logger_info(io->logger, "##  <%d> - Finalizó IO y pasa a READY / SUSP. READY",
              pcb->pid);
}

bool io_sleep_f(t_sleep* peticion, t_io* io_sleep)
{
  bool comms = comunicacion_io_sleep(peticion, io_sleep);
  if (!comms)
  {
    return false;
  }
  finalizar_io(peticion, io_sleep, peticion->pcb);
  return true;
}

void liberar_peticion(void* peticion, t_io* io)
{
  t_pcb* pcb;
  switch (io->tipo_io)
  {
    case E_STDIN:
      t_stdin* pedido1 = (t_stdin*)peticion;
      pcb = pedido1->pcb;
      free(pedido1->peticion);
      free(pedido1);
      break;
    case E_STDOUT:
      t_stdout* pedido2 = (t_stdout*)peticion;
      pcb = pedido2->pcb;
      free(pedido2->peticion);
      free(pedido2);
      break;
    default:
      t_sleep* pedido3 = (t_sleep*)peticion;
      pcb = pedido3->pcb;
      free(pedido3->peticion);
      free(pedido3);
      break;
  }
  cambio_desbloquear(pcb, io->colas);
}

void cerrar_hilo_io(t_io* io)
{
  pthread_mutex_lock(&(io->lista_io->mutex_lista_io));
  while (!list_is_empty(io->lista_io->lista_io))
  {
    void* peticion = list_remove(io->lista_io->lista_io, 0);
    liberar_peticion(peticion, io);
  }
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  pthread_mutex_destroy(&(io->lista_io->mutex_lista_io));
  list_destroy(io->lista_io->lista_io);
  free(io->lista_io);
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
  pthread_mutex_lock(&(io_in->socket_km->mutex_socket));
  envio = envio_stdin(peticion, io_in, buffer);
  if (!envio)
  {
    pthread_mutex_unlock(&(io_in->socket_km->mutex_socket));
    free(buffer);
    cerrar_kernel_scheduler(io_in->socket_server, io_in->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }
  if (!(charla_km_stdin(io_in)))
  {
    pthread_mutex_unlock(&(io_in->socket_km->mutex_socket));
    return false;
  }
  pthread_mutex_unlock(&(io_in->socket_km->mutex_socket));
  finalizar_io(peticion, io_in, peticion->pcb);
  return true;
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
  pthread_mutex_lock(&(io_out->socket_km->mutex_socket));
  bool envio = peticion_stdout_km(peticion, io_out);
  if (!envio)
  {
    cerrar_kernel_scheduler(io_out->socket_server, io_out->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    pthread_mutex_unlock(&(io_out->socket_km->mutex_socket));
    return false;
  }
  // Recibo la respuesta de Kernel Memory
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
  free(recibir_string(io_out->socket_io));
  pthread_mutex_unlock(&(io_out->mutex_socket_io));

  finalizar_io(peticion, io_out, peticion->pcb);
  return true;
}

bool atender_stdin(t_io* io)
{
  t_stdin* peticion = list_get(io->lista_io->lista_io, 0);
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  if (peticion == NULL)
  {
    logger_error(io->logger,
                 "## Error al obtener la peticion de la lista de stdin");
    return false;
  }
  return io_stdin_f(peticion, io);
}

bool atender_stdout(t_io* io)
{
  t_stdout* peticion = list_get(io->lista_io->lista_io, 0);
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  if (peticion == NULL)
  {
    logger_error(io->logger,
                 "## Error al obtener la peticion de la lista de stdout");
    return false;
  }
  return io_stdout_f(peticion, io);
}

bool atender_sleep(t_io* io)
{
  t_sleep* peticion = list_get(io->lista_io->lista_io, 0);
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  if (peticion == NULL)
  {
    logger_error(io->logger,
                 "## Error al obtener la peticion de la lista de sleep");
    return false;
  }
  return io_sleep_f(peticion, io);
}

bool atender_io(t_io* io)
{
  switch (io->tipo_io)
  {
    case E_STDIN:
      return atender_stdin(io);
      break;
    case E_STDOUT:
      return atender_stdout(io);
      break;
    case E_SLEEP:
      return atender_sleep(io);
      break;
  }
  return true;
}

void* hilo_io(void* hilo_io)
{
  t_io* io = (t_io*)hilo_io;
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(io->mutex_fin));
    if (io->cerrar_hilo)
    {
      cerrar_hilo_io(io);
      return NULL;
    }
    pthread_mutex_unlock(&(io->mutex_fin));
    pthread_mutex_lock(&(io->lista_io->mutex_lista_io));
    while (list_is_empty(io->lista_io->lista_io))
    {
      pthread_cond_wait(&(io->nuevo_proceso), &(io->lista_io->mutex_lista_io));
    }
    if (!(atender_io(io)))
    {
      seguir_atendiendo = false;
      logger_error(io->logger, "## Error en la operacion de io de tipo %s",
                   V_TIPO_IO[io->tipo_io]);
    }
  }
  cerrar_hilo_io(io);
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

bool atender_nuevo_io(t_io io[3], int socket_fd, t_colas* colas,
                      bool prioridad_activa)
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
  io[tipo_io].tipo_io = tipo_io;
  io[tipo_io].lista_io = malloc(sizeof(t_lista_io));
  io[tipo_io].lista_io->lista_io = list_create();
  pthread_mutex_init(&(io[tipo_io].lista_io->mutex_lista_io), NULL);

  if (pthread_create(&(io[tipo_io].hilo_io), NULL, hilo_io,
                     (void*)(&(io[tipo_io]))))
  {
    logger_error(colas->logger, "## Error al crear el hilo para IO de tipo %s",
                 V_TIPO_IO[tipo_io]);
    return false;
  }

  return true;
}

bool comparar_prioridad_stdin(void* syscall1, void* syscall2)
{
  t_stdin* stdin1 = (t_stdin*)syscall1;
  t_stdin* stdin2 = (t_stdin*)syscall2;
  return (stdin1->pcb->prioridad <= stdin2->pcb->prioridad);
}
bool comparar_prioridad_stdout(void* syscall1, void* syscall2)
{
  t_stdout* stdout1 = (t_stdout*)syscall1;
  t_stdout* stdout2 = (t_stdout*)syscall2;
  return (stdout1->pcb->prioridad <= stdout2->pcb->prioridad);
}
bool comparar_prioridad_sleep(void* syscall1, void* syscall2)
{
  t_sleep* sleep1 = (t_sleep*)syscall1;
  t_sleep* sleep2 = (t_sleep*)syscall2;
  return (sleep1->pcb->prioridad <= sleep2->pcb->prioridad);
}

void* transformar_peticion(void* peticion, int tipo_io, t_pcb* pcb)
{
  void* pedido;
  switch (tipo_io)
  {
    case E_STDIN:
      t_stdin* stdin = malloc(sizeof(t_stdin));
      stdin->pcb = pcb;
      stdin->peticion = peticion;
      pedido = stdin;
      break;
    case E_STDOUT:
      t_stdout* stdout = malloc(sizeof(t_stdout));
      stdout->pcb = pcb;
      stdout->peticion = peticion;
      pedido = stdout;
      break;
    default:
      t_sleep* sleep = malloc(sizeof(t_sleep));
      sleep->pcb = pcb;
      sleep->peticion = peticion;
      pedido = sleep;
      break;
  }
  return pedido;
}
void agregar_ordenado(void* pedido, t_io* io)
{
  switch (io->tipo_io)
  {
    case E_STDIN:
      list_add_sorted(io->lista_io->lista_io, pedido, comparar_prioridad_stdin);
      break;

    case E_STDOUT:
      list_add_sorted(io->lista_io->lista_io, pedido,
                      comparar_prioridad_stdout);
      break;
    default:
      list_add_sorted(io->lista_io->lista_io, pedido, comparar_prioridad_sleep);
      break;
  }
}
bool procesar_nuevo_io(void* peticion, t_io* io, t_pcb* pcb)
{
  pthread_mutex_lock(&(io->mutex_socket_io));
  if (io->socket_io == -1)
  {
    pthread_mutex_unlock(&(io->mutex_socket_io));
    return false;
  }
  void* pedido = transformar_peticion(peticion, io->tipo_io, pcb);
  pthread_mutex_lock(&(io->lista_io->mutex_lista_io));
  bool lista_vacia = list_is_empty(io->lista_io->lista_io);
  if (io->prioridad_activa)
  {
    agregar_ordenado(pedido, io);
  }
  else
  {
    list_add(io->lista_io->lista_io, pedido);
  }
  if (lista_vacia)
  {
    pthread_cond_signal(&(io->nuevo_proceso));
  }
  pthread_mutex_unlock(&(io->lista_io->mutex_lista_io));
  pthread_mutex_unlock(&(io->mutex_socket_io));
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
    if (io[i].socket_io != -1)
    {
      shutdown(io[i].socket_io, SHUT_RDWR);
      pthread_mutex_lock(&(io[i].mutex_socket_io));
      io[i].socket_io = -1;
      pthread_mutex_unlock(&(io[i].mutex_socket_io));
      pthread_mutex_lock(&(io[i].mutex_fin));
      io[i].cerrar_hilo = true;
      pthread_mutex_lock(&(io[i].mutex_fin));
      pthread_join(io[i].hilo_io, NULL);
      pthread_mutex_lock(&(io[i].mutex_socket_io));
      close(io[i].socket_io);
      pthread_mutex_unlock(&(io[i].mutex_socket_io));
      destruir_io(&io[i]);
    }
  }
}
