#include "ioops.h"

bool run_stdin(t_io* io)
{
  int request_size;
  t_stdin_request* request =
      (t_stdin_request*)receive_buffer(&request_size, io->socket_io);
  log_info(io->logger, "PID %d - IO start", request->pid);

  // Ask for keyboard input.
  log_info(io->logger, "PID %d - Enter %d characters", request->pid,
           request->bytes_to_read);

  char* buffer = NULL;
  size_t buffer_size = 0;

  printf("> ");
  fflush(stdout);

  if (getline(&buffer, &buffer_size, stdin) == -1)
  {
    log_warning(io->logger, "Could not read user input (end of input?)");
    free(request);
    free(buffer);
    return false;
  }

  if (buffer_size >= request->bytes_to_read)
  {
    buffer[request->bytes_to_read - 1] = '\0';
  }

  buffer[strcspn(buffer, "\n")] = '\0';

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
  usleep(request->blocked_time_ms * 1000);  // convert to microseconds

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
