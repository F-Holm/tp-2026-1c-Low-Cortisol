#include "ioops.h"

bool io_tipo_stdin(t_modulo_io* sio)
{
  sio->input = malloc(100);
  scanf("%s", sio->input);
  if (sio->input == NULL)
  {
    free(sio->input);
    return false;
  }
  else
  {
    bool envio_correcto = enviar_string(MID_IO, sio->input, sio->socket_io);
    if (!envio_correcto)
    {
      log_error(sio->logger, "## Error en el Envío de lectura de stdin");
      free(sio->input);
      return false;
    }
    free(sio->input);
    return true;
  }
}