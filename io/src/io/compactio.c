#include "io/compactio.h"

void cerrar_todo(t_modulo_io* modulo_io)
{
  close(modulo_io->socket_io);
  log_destroy(modulo_io->logger);
  config_destroy(modulo_io->config);
}
bool cargar_configs(t_modulo_io* modulo_io)
{
  char* log_levelstr = config_get_string_value(modulo_io->config, "LOG_LEVEL");
  modulo_io->ip = config_get_string_value(modulo_io->config, "IP");
  modulo_io->puerto = config_get_string_value(modulo_io->config, "PORT");
  t_log_level log_level = log_level_from_string(log_levelstr);
  modulo_io->logger = log_create("io.log", "IO", true, log_level);
  if (modulo_io->logger == NULL)
  {
    config_destroy(modulo_io->config);
    return false;
  }
  return true;
}
bool iniciar_enviar_tipo_io(t_modulo_io* modulo_io)
{
  modulo_io->socket_io =
      crear_conexion(modulo_io->ip, modulo_io->puerto);  // Establezco conexión

  if (modulo_io->socket_io == -1)
  {
    log_error(modulo_io->logger, "#ERROR DE CONEXION");
    cerrar_todo(modulo_io);
    return false;
  }
  log_info(modulo_io->logger, "## Conectado a Kernel Scheduler");

  // Handshake con Kernel Scheduler
  bool envio_correcto = enviar_handshake(MID_IO, modulo_io->socket_io);
  if (!envio_correcto)
  {
    log_error(modulo_io->logger,
              "## Error en el Handshake con Kernel Scheduler");
    cerrar_todo(modulo_io);
    return false;
  }

  int recepcion_correcta = recibir_handshake(modulo_io->socket_io);
  if (recepcion_correcta != MID_KERNEL_SCHEDULER)
  {
    log_error(modulo_io->logger,
              "## Error en el Handshake con Kernel Scheduler");
    cerrar_todo(modulo_io);
    return false;
  }
  log_info(modulo_io->logger, "## Handshake exitoso con Kernel Scheduler");

  envio_correcto = enviar_string(
      OP_TIPO_IO, (char*)V_TIPO_IO[modulo_io->tipo_io], modulo_io->socket_io);
  if (!envio_correcto)
  {
    log_error(modulo_io->logger, "## Error en el Envío de tipo de IO");
    cerrar_todo(modulo_io);
    return false;
  }
  log_info(modulo_io->logger, "## Envio correcto de tipo de IO");
  return true;
}

bool args(int argc, char** argv, t_modulo_io* modulo_io)
{
  if (argc != 3)
  {
    return false;
  }
  char* archivo_config = argv[1];
  modulo_io->config = config_create(archivo_config);
  // Chequeo si la operacion de IO recibida existe
  if (strcmp(V_TIPO_IO[E_STDIN], argv[2]) == 0)
  {
    modulo_io->tipo_io = E_STDIN;
  }
  else if (strcmp(V_TIPO_IO[E_STDOUT], argv[2]) == 0)
  {
    modulo_io->tipo_io = E_STDOUT;
  }
  else if (strcmp(V_TIPO_IO[E_SLEEP], argv[2]) == 0)
  {
    modulo_io->tipo_io = E_SLEEP;
  }
  else
  {
    return false;
  }

  return true;
}