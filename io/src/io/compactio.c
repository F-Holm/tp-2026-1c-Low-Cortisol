#include "io/compactio.h"

void cerrar_todo(t_log* logger, t_config* config, int socket_io)
{
  close(socket_io);
  log_destroy(logger);
  config_destroy(config);
}