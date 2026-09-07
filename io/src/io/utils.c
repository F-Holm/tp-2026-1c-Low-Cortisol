#include "io/utils.h"

void close_io(t_io* io)
{
  close(io->socket_io);
  log_destroy(io->logger);
  config_destroy(io->config);
}

bool load_config(t_io* io)
{
  char* log_level_str = config_get_string_value(io->config, "LOG_LEVEL");
  io->ip = config_get_string_value(io->config, "KERNEL_SCHEDULER_IP");
  io->port = config_get_string_value(io->config, "KERNEL_SCHEDULER_PORT");
  t_log_level log_level = log_level_from_string(log_level_str);
  io->logger = log_create("io.log", "IO", true, log_level, false);
  if (io->logger == NULL)
  {
    config_destroy(io->config);
    return false;
  }
  log_debug(io->logger,
            "Config loaded: %s interface, Kernel Scheduler at %s:%s",
            IO_TYPE_NAMES[io->io_type], io->ip, io->port);
  return true;
}

bool connect_to_scheduler(t_io* io)
{
  io->socket_io = create_connection(io->ip, io->port);

  if (io->socket_io == -1)
  {
    log_error(io->logger, "## Connection error with Kernel Scheduler");
    close_io(io);
    return false;
  }
  log_info(io->logger, "## Connected to Kernel Scheduler");

  bool sent_ok = send_handshake(MID_IO, io->socket_io);
  if (!sent_ok)
  {
    log_error(io->logger, "## Handshake error with Kernel Scheduler");
    close_io(io);
    return false;
  }

  int received_id = receive_handshake(io->socket_io);
  if (received_id != MID_KERNEL_SCHEDULER)
  {
    log_error(io->logger, "## Handshake error with Kernel Scheduler");
    close_io(io);
    return false;
  }
  log_debug(io->logger, "Handshake successful with Kernel Scheduler");

  sent_ok =
      send_string(OP_IO_TYPE, (char*)IO_TYPE_NAMES[io->io_type], io->socket_io);
  if (!sent_ok)
  {
    log_error(io->logger, "## Error sending the IO type");
    close_io(io);
    return false;
  }
  log_debug(io->logger, "IO type sent successfully");
  return true;
}

bool parse_args(int argc, char** argv, t_io* io)
{
  if (argc != 3)
  {
    return false;
  }
  char* config_path = argv[1];
  io->config = config_create(config_path);
  // Check that the received IO operation exists.
  if (strcmp(IO_TYPE_NAMES[E_STDIN], argv[2]) == 0)
  {
    io->io_type = E_STDIN;
  }
  else if (strcmp(IO_TYPE_NAMES[E_STDOUT], argv[2]) == 0)
  {
    io->io_type = E_STDOUT;
  }
  else if (strcmp(IO_TYPE_NAMES[E_SLEEP], argv[2]) == 0)
  {
    io->io_type = E_SLEEP;
  }
  else
  {
    return false;
  }

  return true;
}
