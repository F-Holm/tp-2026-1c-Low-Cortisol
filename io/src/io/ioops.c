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
    buffer[peticion_stdin->tamanio_a_leer - 1] =
        '\0';  // Trunco el buffer si es necesario
  }

  // FALTA ARREGLAR ESTO
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