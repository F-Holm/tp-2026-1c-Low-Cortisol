#include "ioops.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/utils.h"
#include "utils/collections/list.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/syscalls.h"
#include "utils/time.h"

bool run_stdin(t_io* io)
{
  int request_size;
  t_stdin_request* request =
      (t_stdin_request*)receive_buffer(&request_size, io->socket_io);
  log_info(io->logger, "PID %d - IO start", request->pid);

  // Ask for keyboard input.
  log_info(io->logger, "PID %d - Requesting %d characters", request->pid,
           request->bytes_to_read);

  printf("> ");
  fflush(stdout);

  char* buffer = file_read_line(stdin);
  if (buffer == NULL)
  {
    log_warning(io->logger, "Could not read user input (end of input?)");
    free(request);
    return false;
  }

  if (request->bytes_to_read > 0 && strlen(buffer) >= request->bytes_to_read)
  {
    buffer[request->bytes_to_read - 1] = '\0';
  }

  bool sent_ok = send_string(OP_STDIN_RESPONSE, buffer, io->socket_io);
  if (!sent_ok)
  {
    log_warning(
        io->logger,
        "## Could not send the IO response to Kernel Scheduler (peer gone?)");
    free(buffer);
    free(request);
    return false;
  }
  log_info(io->logger, "PID %d - IO end", request->pid);

  free(request);
  free(buffer);
  return true;
}

bool run_stdout(t_io* io)
{
  t_list* packet = receive_packet(io->socket_io);
  t_stdout_request* request = (t_stdout_request*)list_remove(packet, 0);
  char* buffer = (char*)list_remove(packet, 0);
  list_destroy(packet);
  if (buffer == NULL)
  {
    log_error(io->logger, "Malformed STDOUT request: no content");
    free(request);
    return false;
  }
  log_info(io->logger, "PID %d - IO start", request->pid);

  // Print the received message on screen.
  log_info(io->logger, "PID: %d - %s", request->pid, buffer);

  // Reply OK to the scheduler so it knows the IO is done.
  bool sent_ok = send_string(OP_STDOUT_RESPONSE, "OK", io->socket_io);
  if (!sent_ok)
  {
    log_warning(
        io->logger,
        "## Could not send the IO response to Kernel Scheduler (peer gone?)");
    free(request);
    free(buffer);
    return false;
  }
  log_info(io->logger, "PID %d - IO end", request->pid);
  free(request);
  free(buffer);
  return true;
}

bool run_sleep(t_io* io)
{
  int request_size;
  t_sleep_request* request =
      (t_sleep_request*)receive_buffer(&request_size, io->socket_io);
  log_info(io->logger, "PID %d - IO start", request->pid);

  // Simulate the sleep.
  log_info(io->logger, "PID: %d - Sleeping for %d seconds", request->pid,
           request->blocked_time_ms / 1000);
  time_sleep_ms(request->blocked_time_ms);

  // Reply OK to the scheduler so it knows the IO is done.
  bool sent_ok = send_string(OP_SLEEP_RESPONSE, "OK", io->socket_io);
  if (!sent_ok)
  {
    log_warning(
        io->logger,
        "## Could not send the IO response to Kernel Scheduler (peer gone?)");
    free(request);
    return false;
  }
  log_info(io->logger, "PID %d - IO end", request->pid);
  free(request);
  return true;
}
