#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "io/ioops.h"
#include "io/utils.h"
#include "utils/config.h"
#include "utils/io.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_io io;

  if (!parse_args(argc, argv, &io))
  {
    return EXIT_FAILURE;
  }

  if (!load_config(&io))
  {
    return EXIT_FAILURE;
  }
  if (!connect_to_scheduler(&io))
  {
    return EXIT_FAILURE;
  }
  // Waiting for instructions from the Kernel Scheduler.
  bool keep_running = true;
  bool ok = -1;
  while (keep_running)
  {
    int op_code;
    op_code = receive_op_code(io.socket_io);
    switch (op_code)
    {
      case OP_IO_STDIN_REQUEST:
        ok = run_stdin(&io);
        if (!ok)
        {
          keep_running = false;
        }
        break;

      case OP_IO_STDOUT_REQUEST:
        ok = run_stdout(&io);
        if (!ok)
        {
          keep_running = false;
        }
        break;

      case OP_IO_SLEEP_REQUEST:
        ok = run_sleep(&io);
        if (!ok)
        {
          keep_running = false;
        }
        break;

      default:
        keep_running = false;
    }
  }
  log_info(io.logger, " IO shutdown");
  close_io(&io);
  return EXIT_SUCCESS;
}
