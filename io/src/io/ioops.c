#include "ioops.h"

bool io_tipo_stdin(t_modulo_io* sio)
{
  int size_peticion;
  t_stdin_request* peticion_stdin =
      (t_stdin_request*)receive_buffer(&size_peticion, sio->socket_io);
  log_info(sio->logger, "## PID %d -Inicio de IO", peticion_stdin->pid);

  // Solicito el input por teclado
  log_info(sio->logger, "## PID %d -Ingrese %d caracteres", peticion_stdin->pid,
           peticion_stdin->bytes_to_read);

  char* buffer = NULL;
  size_t tamanio = 0;

  printf("> ");
  fflush(stdout);

  if (getline(&buffer, &tamanio, stdin) == -1)
  {
    log_error(sio->logger, " Error al leer el input del usuario");
    free(peticion_stdin);
    free(buffer);
    return false;
  }

  if (tamanio >= peticion_stdin->bytes_to_read)
  {
    buffer[peticion_stdin->bytes_to_read - 1] = '\0';
  }

  buffer[strcspn(buffer, "\n")] = '\0';

  bool envio_correcto = send_string(OP_STDIN_RESPONSE, buffer, sio->socket_io);
  if (!envio_correcto)
  {
    log_error(sio->logger,
              " Error al enviar la respuesta de IO a Kernel Scheduler");
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
  t_list* packet = receive_packet(sio->socket_io);
  t_stdout_request* peticion = (t_stdout_request*)list_remove(packet, 0);
  char* buffer = (char*)list_remove(packet, 0);
  list_destroy(packet);
  if (buffer == NULL)
  {
    log_error(sio->logger,
              " Error - No se recibió nada para escribir en pantalla");
    free(peticion);
    return false;
  }
  log_info(sio->logger, "## PID %d -Inicio de IO", peticion->pid);

  // Imprimo por pantalla el mensaje recibido
  log_info(sio->logger, "## PID: %d - %s", peticion->pid, buffer);

  // Envio OK a Scheduler para que sepa que ya terminó el IO
  bool envio_correcto = send_string(OP_STDOUT_RESPONSE, "OK", sio->socket_io);
  if (!envio_correcto)
  {
    log_error(sio->logger,
              " Error al enviar la respuesta de IO a Kernel Scheduler");
    free(peticion);
    free(buffer);
    return false;
  }
  log_info(sio->logger, "## PID %d -Fin de IO", peticion->pid);
  free(peticion);
  free(buffer);
  return true;
}

bool io_tipo_sleep(t_modulo_io* sio)
{
  // Recibo la peticion de IO
  int size_peticion;
  t_sleep_request* peticion_sleep =
      (t_sleep_request*)receive_buffer(&size_peticion, sio->socket_io);
  log_info(sio->logger, "## PID %d -Inicio de IO", peticion_sleep->pid);

  // Simulo el sleep
  log_info(sio->logger, "## PID: %d - Haciendo sleep por %d segundos",
           peticion_sleep->pid, peticion_sleep->blocked_time_ms / 1000);
  usleep(peticion_sleep->blocked_time_ms * 1000);  // Convertir a microsegundos

  // Envio OK a Scheduler para que sepa que ya terminó el IO
  bool envio_correcto = send_string(OP_SLEEP_RESPONSE, "OK", sio->socket_io);
  if (!envio_correcto)
  {
    log_error(sio->logger,
              " Error al enviar la respuesta de IO a Kernel Scheduler");
    free(peticion_sleep);
    return false;
  }
  log_info(sio->logger, "## PID %d -Fin de IO", peticion_sleep->pid);
  free(peticion_sleep);
  return true;
}