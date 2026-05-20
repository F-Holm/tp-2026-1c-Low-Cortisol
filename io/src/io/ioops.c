#include "ioops.h"

bool io_tipo_stdin(t_modulo_io* sio)
{
  int size_peticion;
  t_peticion_stdin* peticion_stdin =
      (t_peticion_stdin*)recibir_buffer(&size_peticion, sio->socket_io);
  char* buffer = malloc(peticion_stdin->tamanio_a_leer);
  log_info(sio->logger, "## PID %d -Inicio de IO", peticion_stdin->pid);

  // Solicito el input por teclado
  log_info(sio->logger, "PID %d -Ingrese %d caracteres", peticion_stdin->pid,
           peticion_stdin->tamanio_a_leer);

  buffer = readline(">");
  if (buffer == NULL)
  {
    log_error(sio->logger, "## Error al leer el input del usuario");
    free(peticion_stdin);
    return false;
  }
  else if (strlen(buffer) >= peticion_stdin->tamanio_a_leer)
  {
    buffer[peticion_stdin->tamanio_a_leer - 1] = '\0';
    // Trunco el buffer si es necesario
  }

  bool envio_correcto =
      enviar_string(OP_RESPUESTA_STDIN, buffer, sio->socket_io);
  if (!envio_correcto)
  {
    log_error(sio->logger,
              "## Error al enviar la respuesta de IO a Kernel Scheduler");
    free(buffer);
    free(peticion_stdin);
    return false;
  }
  log_info(sio->logger, "## PID %d -Fin de IO", peticion_stdin->pid);

  free(peticion_stdin);
  free(buffer);
  return true;
}

bool io_tipo_stdout(t_modulo_io* sio)
{
  // Recibo la peticion de IO
  int size_peticion;
  t_peticion_stdout* peticion_stdout =
      (t_peticion_stdout*)recibir_buffer(&size_peticion, sio->socket_io);
  char* buffer = recibir_string(sio->socket_io);
  if (buffer == NULL)
  {
    log_error(sio->logger,
              "## Error - No se recibió nada para escribir en pantalla");
    free(peticion_stdout);
    return false;
  }
  log_info(sio->logger, "## PID %d -Inicio de IO", peticion_stdout->pid);

  // Imprimo por pantalla el mensaje recibido
  log_info(sio->logger, "## PID: %d - %s", peticion_stdout->pid, buffer);

  // Envio OK a Scheduler para que sepa que ya terminó el IO
  bool envio_correcto =
      enviar_string(OP_RESPUESTA_STDOUT, "OK", sio->socket_io);
  if (!envio_correcto)
  {
    log_error(sio->logger,
              "## Error al enviar la respuesta de IO a Kernel Scheduler");
    free(peticion_stdout);
    free(buffer);
    return false;
  }
  log_info(sio->logger, "## PID %d -Fin de IO", peticion_stdout->pid);
  free(peticion_stdout);
  free(buffer);
  return true;
}

bool io_tipo_sleep(t_modulo_io* sio)
{
  // Recibo la peticion de IO
  int size_peticion;
  t_peticion_sleep* peticion_sleep =
      (t_peticion_sleep*)recibir_buffer(&size_peticion, sio->socket_io);
  log_info(sio->logger, "## PID %d -Inicio de IO", peticion_sleep->pid);

  // Simulo el sleep
  log_info(sio->logger, "## PID: %d - Haciendo sleep por %d segundos",
           peticion_sleep->pid, peticion_sleep->tiempo_bloqueado);
  usleep(peticion_sleep->tiempo_bloqueado * 1000);  // Convertir a microsegundos

  // Envio OK a Scheduler para que sepa que ya terminó el IO
  bool envio_correcto = enviar_string(OP_RESPUESTA_SLEEP, "OK", sio->socket_io);
  if (!envio_correcto)
  {
    log_error(sio->logger,
              "## Error al enviar la respuesta de IO a Kernel Scheduler");
    free(peticion_sleep);
    return false;
  }
  log_info(sio->logger, "## PID %d -Fin de IO", peticion_sleep->pid);
  free(peticion_sleep);
  return true;
}