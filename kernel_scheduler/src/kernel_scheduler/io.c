#include "kernel_scheduler/io.h"

#include <string.h>

#include "kernel_scheduler/common/handshake.h"
#include "kernel_scheduler/shutdown.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "utils/io.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"
#include "utils/syscalls.h"

static bool send_stdout(t_io* io_out, t_stdout* request, char* buffer);
static bool request_stdout_km(t_stdout* request, t_io* io_out);
static bool send_stdin(t_stdin* request, t_io* io_in, char* buffer);
static bool communication_io_stdin(t_stdin* request, t_io* io_in,
                                   char** buffer);
static bool communication_io_sleep(t_sleep* request, t_io* io_sleep);
static void finalize_io(void* request, t_io* io, t_pcb* pcb);
static bool io_sleep_f(t_sleep* request, t_io* io_sleep);
static void free_request(void* request, t_io* io);
static void close_thread_io(t_io* io);
static bool chat_km_stdin(t_io* io_in);
static int io_stdin_f(t_stdin* request, t_io* io_in);
static bool receive_km_stdout(t_io* io_out);
static bool io_stdout_f(t_stdout* request, t_io* io_out);
static bool handle_stdin(t_io* io);
static bool handle_stdout(t_io* io);
static bool handle_sleep(t_io* io);
static bool handle_io(t_io* io);
static void* io_thread(void* io_thread);
static int get_io_type(int socket_fd, t_log* logger);
static bool compare_priority_stdin(void* syscall1, void* syscall2);
static bool compare_priority_stdout(void* syscall1, void* syscall2);
static bool compare_priority_sleep(void* syscall1, void* syscall2);
static void* transform_request(void* request, int io_type, t_pcb* pcb);
static void add_ordered(void* entry, t_io* io);
static void destroy_io(t_io* io);

t_io* create_estructuras_io(void)
{
  t_io* io = malloc(sizeof(t_io) * 3);
  for (int i = 0; i < 3; i++)
  {
    io[i].socket_io = -1;
    pthread_cond_init(&(io[i].new_process), NULL);
  }
  return io;
}

bool handle_new_io(t_io io[3], int socket_fd, t_queues* queues,
                   bool priority_active, int socket_server)
{
  if (!respond_handshake(socket_fd, MID_KERNEL_SCHEDULER, queues->logger))
    return false;

  int io_type = get_io_type(socket_fd, queues->logger);
  if (io_type == -1)
    return false;

  if (io[io_type].socket_io != -1)
  {
    log_warning(queues->logger, "Duplicate IO type: %d. Closing connection",
                io_type);
    close(socket_fd);
    return false;
  }
  // Prepare the t_io to create the thread
  io[io_type].socket_io = socket_fd;
  io[io_type].current_process = NULL;
  io[io_type].socket_server = socket_server;
  io[io_type].queues = queues;
  io[io_type].logger = queues->logger;
  io[io_type].km_socket = queues->km_socket;
  io[io_type].priority_active = priority_active;
  io[io_type].io_type = io_type;
  io[io_type].io_list = malloc(sizeof(t_io_list));
  io[io_type].io_list->io_list = list_create();
  pthread_mutex_init(&(io[io_type].io_list->io_list_mutex), NULL);
  atomic_init(&(io[io_type].close_thread), false);

  if (pthread_create(&(io[io_type].io_thread), NULL, io_thread,
                     (void*)(&(io[io_type]))))
  {
    log_error(queues->logger, "Error creating the thread for IO of type %s",
              IO_TYPE_NAMES[io_type]);
    return false;
  }
  log_debug(queues->logger, "IO thread of type %s created",
            IO_TYPE_NAMES[io_type]);
  return true;
}

bool procesar_new_io(void* request, t_io* io, t_pcb* pcb)
{
  if (atomic_load(&(io->close_thread)))
  {
    return false;
  }
  void* entry = transform_request(request, io->io_type, pcb);
  pthread_mutex_lock(&(io->io_list->io_list_mutex));
  bool list_empty = list_is_empty(io->io_list->io_list);
  if (io->priority_active)
  {
    add_ordered(entry, io);
  }
  else
  {
    list_add(io->io_list->io_list, entry);
  }
  if (list_empty)
  {
    pthread_cond_signal(&(io->new_process));
  }
  pthread_mutex_unlock(&(io->io_list->io_list_mutex));
  return true;
}

void close_io(t_io* io)
{
  for (int i = 0; i < 3; i++)
  {
    if (io[i].socket_io != -1)
    {
      if (!atomic_exchange(&(io[i].close_thread), true))
      {
        pthread_mutex_lock(&(io[i].io_list->io_list_mutex));
        pthread_cond_signal(&(io[i].new_process));
        pthread_mutex_unlock(&(io[i].io_list->io_list_mutex));
      }
      shutdown(io[i].socket_io, SHUT_RDWR);
      pthread_join(io[i].io_thread, NULL);
      close(io[i].socket_io);
      destroy_io(&(io[i]));
    }
  }
  free(io);
}

static bool send_stdout(t_io* io_out, t_stdout* request, char* buffer)
{
  int request_size = sizeof(t_stdout_request);
  t_packet* packet = create_packet(OP_IO_STDOUT_REQUEST);
  packet_append(packet, request->request, request_size);
  packet_append_string(packet, buffer);
  bool send = send_packet(packet, io_out->socket_io);
  destroy_packet(packet);
  free(buffer);
  if (!send)
  {
    log_warning(io_out->logger, "Error sending Kernel Memory's response to IO");
    return false;
  }
  return true;
}

static bool request_stdout_km(t_stdout* request, t_io* io_out)
{
  int request_size = sizeof(t_stdout_request);
  bool send = send_buffer(OP_IO_STDOUT_REQUEST, request->request, request_size,
                          io_out->km_socket->km_socket);
  if (!send)
  {
    log_warning(io_out->logger, "Error communicating with Kernel Memory");
    return false;
  }
  return true;
}

static bool send_stdin(t_stdin* request, t_io* io_in, char* buffer)
{
  int request_size = sizeof(t_stdin_request);
  t_packet* packet = create_packet(OP_IO_STDIN_REQUEST);
  packet_append(packet, request->request, request_size);
  packet_append_string(packet, buffer);
  bool send = send_packet(packet, io_in->km_socket->km_socket);
  destroy_packet(packet);
  if (!send)
  {
    log_warning(io_in->logger, "Error sending to Kernel Memory");
    return false;
  }
  free(buffer);
  return true;
}

static bool communication_io_stdin(t_stdin* request, t_io* io_in, char** buffer)
{
  int request_size = sizeof(t_stdin_request);
  bool send = send_buffer(OP_IO_STDIN_REQUEST, request->request, request_size,
                          io_in->socket_io);
  if (!send)
  {
    log_warning(io_in->logger, "Error sending to IO");

    return false;
  }

  // Receive the IO response
  int cod_op = receive_op_code(io_in->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    return false;
  }
  *buffer = receive_string(io_in->socket_io);

  if (*buffer == NULL)
  {
    log_warning(io_in->logger, "Error receiving the IO response");
    free(*buffer);
    return false;
  }
  return true;
}

static bool communication_io_sleep(t_sleep* request, t_io* io_sleep)
{
  int request_size = sizeof(t_sleep_request);
  bool send = send_buffer(OP_IO_SLEEP_REQUEST, request->request, request_size,
                          io_sleep->socket_io);
  if (!send)
  {
    log_warning(io_sleep->logger, "Error sending to IO");
    return false;
  }

  int cod_op = receive_op_code(io_sleep->socket_io);
  if (cod_op == OP_CODE_ERROR)
  {
    log_warning(io_sleep->logger,
                "Error in IO's response to the Kernel Scheduler");
    return false;
  }
  char* response = receive_string(io_sleep->socket_io);
  if (strcmp(response, "OK") != 0)
  {
    log_warning(io_sleep->logger,
                "Error in IO's response to the Kernel Scheduler. Expected: OK");
    free(response);
    return false;
  }
  free(response);
  return true;
}

// IO termination helper
static void finalize_io(void* request, t_io* io, t_pcb* pcb)
{
  pthread_mutex_lock(&(io->io_list->io_list_mutex));
  if (list_remove_element(io->io_list->io_list, request) == 0)
  {
    pthread_mutex_unlock(&(io->io_list->io_list_mutex));
    log_error(io->logger, "Error removing the process from the IO list");
    return;
  }
  pthread_mutex_unlock(&(io->io_list->io_list_mutex));
  log_debug(io->logger, "PID %d - Removed from the IO list", pcb->pid);

  // Move to ready or susp ready depending on the blocked time
  free_request(request, io);
  log_info(io->logger, "<%d> - Finished IO and moves to READY / SUSP. READY",
           pcb->pid);
}

static bool io_sleep_f(t_sleep* request, t_io* io_sleep)
{
  bool comms = communication_io_sleep(request, io_sleep);
  if (!comms)
  {
    return false;
  }
  finalize_io(request, io_sleep, request->pcb);
  return true;
}

static void free_request(void* request, t_io* io)
{
  t_pcb* pcb;
  switch (io->io_type)
  {
    case E_STDIN:
      t_stdin* entry1 = (t_stdin*)request;
      pcb = entry1->pcb;
      free(entry1->request);
      free(entry1);
      break;
    case E_STDOUT:
      t_stdout* entry2 = (t_stdout*)request;
      pcb = entry2->pcb;
      free(entry2->request);
      free(entry2);
      break;
    default:
      t_sleep* entry3 = (t_sleep*)request;
      pcb = entry3->pcb;
      free(entry3->request);
      free(entry3);
      break;
  }
  transition_unlock(pcb, io->queues);
}

static void close_thread_io(t_io* io)
{
  atomic_store(&(io->close_thread), true);
  pthread_mutex_lock(&(io->io_list->io_list_mutex));
  while (!list_is_empty(io->io_list->io_list))
  {
    void* request = list_remove(io->io_list->io_list, 0);
    free_request(request, io);
  }
  pthread_mutex_unlock(&(io->io_list->io_list_mutex));
  pthread_mutex_destroy(&(io->io_list->io_list_mutex));
  list_destroy(io->io_list->io_list);
  free(io->io_list);
}

static bool chat_km_stdin(t_io* io_in)
{
  int cod_op = -1;
  cod_op = receive_op_code(io_in->km_socket->km_socket);
  switch (cod_op)
  {
    case OP_MEMORY_CORRUPTED:
      free(receive_string(io_in->km_socket->km_socket));
      close_kernel_scheduler(io_in->socket_server, io_in->logger,
                             SR_CORRUPTED_MEMORY, -1);
      return false;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(io_in->km_socket->km_socket));
      create_resumption_routine_thread(io_in->queues);
      return chat_km_stdin(io_in);
    case OP_STDIN_RESPONSE:
      free(receive_string(io_in->km_socket->km_socket));
      return true;
    default:
      free(receive_string(io_in->km_socket->km_socket));
      close_kernel_scheduler(io_in->socket_server, io_in->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      return false;
  }
}

static int io_stdin_f(t_stdin* request, t_io* io_in)
{
  // Send request to IO
  char* buffer;
  bool send = communication_io_stdin(request, io_in, &buffer);
  if (!send)
  {
    return false;
  }
  // Send the packet to Kernel Memory so it writes to memory
  pthread_mutex_lock(&(io_in->km_socket->socket_mutex));
  send = send_stdin(request, io_in, buffer);
  if (!send)
  {
    close_kernel_scheduler(io_in->socket_server, io_in->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           io_in->km_socket->km_socket);
    pthread_mutex_unlock(&(io_in->km_socket->socket_mutex));
    free(buffer);
    return false;
  }
  if (!(chat_km_stdin(io_in)))
  {
    pthread_mutex_unlock(&(io_in->km_socket->socket_mutex));
    return false;
  }
  pthread_mutex_unlock(&(io_in->km_socket->socket_mutex));
  finalize_io(request, io_in, request->pcb);
  return true;
}

static bool receive_km_stdout(t_io* io_out)
{
  int op_code = -1;
  op_code = receive_op_code(io_out->km_socket->km_socket);

  switch (op_code)
  {
    case OP_MEMORY_CORRUPTED:
      free(receive_string(io_out->km_socket->km_socket));
      close_kernel_scheduler(io_out->socket_server, io_out->logger,
                             SR_CORRUPTED_MEMORY, -1);
      return false;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(io_out->km_socket->km_socket));
      create_resumption_routine_thread(io_out->queues);
      return receive_km_stdout(io_out);
    case OP_STDOUT_RESPONSE:
      return true;
    default:
      free(receive_string(io_out->km_socket->km_socket));
      close_kernel_scheduler(io_out->socket_server, io_out->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      return false;
  }
}

static bool io_stdout_f(t_stdout* request, t_io* io_out)
{
  int cod_op = -1;
  // Send request to Kernel Memory so it reads from memory
  pthread_mutex_lock(&(io_out->km_socket->socket_mutex));
  bool send = request_stdout_km(request, io_out);
  if (!send)
  {
    close_kernel_scheduler(io_out->socket_server, io_out->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           io_out->km_socket->km_socket);
    pthread_mutex_unlock(&(io_out->km_socket->socket_mutex));
    return false;
  }
  // Receive the Kernel Memory response
  if (!(receive_km_stdout(io_out)))
  {
    pthread_mutex_unlock(&(io_out->km_socket->socket_mutex));
    return false;
  }

  char* buffer = receive_string(io_out->km_socket->km_socket);
  pthread_mutex_unlock(&(io_out->km_socket->socket_mutex));
  if (buffer == NULL)
  {
    log_warning(io_out->logger, "Error receiving the Kernel Memory response");
    free(buffer);
    close_kernel_scheduler(io_out->socket_server, io_out->logger,
                           SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
    return false;
  }

  // Send the message and the request to IO so it prints to screen
  send = send_stdout(io_out, request, buffer);
  if (!send)
  {
    return false;
  }

  cod_op = receive_op_code(io_out->socket_io);
  if (cod_op != OP_STDOUT_RESPONSE)
  {
    log_warning(io_out->logger,
                "Error in IO's response to the Kernel Scheduler");
    return false;
  }
  free(receive_string(io_out->socket_io));
  finalize_io(request, io_out, request->pcb);
  return true;
}

static bool handle_stdin(t_io* io)
{
  t_stdin* request = list_get(io->io_list->io_list, 0);
  pthread_mutex_unlock(&(io->io_list->io_list_mutex));
  if (request == NULL)
  {
    log_warning(io->logger, "The STDIN request queue was empty");
    return false;
  }
  return io_stdin_f(request, io);
}

static bool handle_stdout(t_io* io)
{
  t_stdout* request = list_get(io->io_list->io_list, 0);
  pthread_mutex_unlock(&(io->io_list->io_list_mutex));
  if (request == NULL)
  {
    log_warning(io->logger, "The STDOUT request queue was empty");
    return false;
  }
  return io_stdout_f(request, io);
}

static bool handle_sleep(t_io* io)
{
  t_sleep* request = list_get(io->io_list->io_list, 0);
  pthread_mutex_unlock(&(io->io_list->io_list_mutex));
  if (request == NULL)
  {
    log_warning(io->logger, "The SLEEP request queue was empty");
    return false;
  }
  return io_sleep_f(request, io);
}

static bool handle_io(t_io* io)
{
  switch (io->io_type)
  {
    case E_STDIN:
      return handle_stdin(io);
      break;
    case E_STDOUT:
      return handle_stdout(io);
      break;
    case E_SLEEP:
      return handle_sleep(io);
      break;
  }
  return true;
}

static void* io_thread(void* io_thread)
{
  t_io* io = (t_io*)io_thread;
  bool seguir_atendiendo = true;
  while (seguir_atendiendo)
  {
    pthread_mutex_lock(&(io->io_list->io_list_mutex));
    while (!atomic_load(&(io->close_thread)) &&
           list_is_empty(io->io_list->io_list))
    {
      pthread_cond_wait(&(io->new_process), &(io->io_list->io_list_mutex));
    }
    if (atomic_load(&(io->close_thread)))
    {
      pthread_mutex_unlock(&(io->io_list->io_list_mutex));
      close_thread_io(io);
      return NULL;
    }

    if (!(handle_io(io)))
    {
      seguir_atendiendo = false;
      log_warning(io->logger, "IO operation of type %s failed",
                  IO_TYPE_NAMES[io->io_type]);
    }
  }
  close_thread_io(io);
  return NULL;
}

static int get_io_type(int socket_fd, t_log* logger)
{
  if (receive_op_code(socket_fd) != OP_IO_TYPE)
  {
    log_error(logger, "Wrong operation type. Expected: OP_IO_TYPE");
    return -1;
  }

  char* buffer = receive_string(socket_fd);
  int io_type;

  if (strcmp(buffer, IO_TYPE_NAMES[E_STDIN]) == 0)
    io_type = E_STDIN;
  else if (strcmp(buffer, IO_TYPE_NAMES[E_STDOUT]) == 0)
    io_type = E_STDOUT;
  else if (strcmp(buffer, IO_TYPE_NAMES[E_SLEEP]) == 0)
    io_type = E_SLEEP;
  else
  {
    log_error(logger, "Invalid IO type: %s", buffer);
    free(buffer);
    return -1;
  }
  log_info(logger, "IO of type %s connected", buffer);
  free(buffer);
  return io_type;
}

static bool compare_priority_stdin(void* syscall1, void* syscall2)
{
  t_stdin* stdin1 = (t_stdin*)syscall1;
  t_stdin* stdin2 = (t_stdin*)syscall2;
  return (stdin1->pcb->priority <= stdin2->pcb->priority);
}

static bool compare_priority_stdout(void* syscall1, void* syscall2)
{
  t_stdout* stdout1 = (t_stdout*)syscall1;
  t_stdout* stdout2 = (t_stdout*)syscall2;
  return (stdout1->pcb->priority <= stdout2->pcb->priority);
}

static bool compare_priority_sleep(void* syscall1, void* syscall2)
{
  t_sleep* sleep1 = (t_sleep*)syscall1;
  t_sleep* sleep2 = (t_sleep*)syscall2;
  return (sleep1->pcb->priority <= sleep2->pcb->priority);
}

static void* transform_request(void* request, int io_type, t_pcb* pcb)
{
  void* entry;
  switch (io_type)
  {
    case E_STDIN:
      t_stdin* stdin = malloc(sizeof(t_stdin));
      stdin->pcb = pcb;
      stdin->request = request;
      entry = stdin;
      break;
    case E_STDOUT:
      t_stdout* stdout = malloc(sizeof(t_stdout));
      stdout->pcb = pcb;
      stdout->request = request;
      entry = stdout;
      break;
    default:
      t_sleep* sleep = malloc(sizeof(t_sleep));
      sleep->pcb = pcb;
      sleep->request = request;
      entry = sleep;
      break;
  }
  return entry;
}

static void add_ordered(void* entry, t_io* io)
{
  switch (io->io_type)
  {
    case E_STDIN:
      list_add_sorted(io->io_list->io_list, entry, compare_priority_stdin);
      break;

    case E_STDOUT:
      list_add_sorted(io->io_list->io_list, entry, compare_priority_stdout);
      break;
    default:
      list_add_sorted(io->io_list->io_list, entry, compare_priority_sleep);
      break;
  }
}

static void destroy_io(t_io* io)
{
  pthread_cond_destroy(&(io->new_process));
  close(io->socket_io);
}
