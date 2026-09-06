#include "kernel_scheduler/misc.h"

#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "utils/msg.h"

const char* const STATE_NAMES[7] = {
    "NEW", "READY", "EXEC", "BLOCK", "SUSP. BLOCK", "SUSP. READY", "EXIT"};

const char* const SHUTDOWN_REASONS[4] = {
    "Processes finished successfully", "BSOD: Corruption of memory detected",
    "Connection error with Kernel Memory", "Unknown error"};

static pthread_mutex_t mutex_pid_pcb;

static pthread_mutex_t mutex_shutdown;

static bool is_highest_priority(void* pcb1, void* pcb2);
static void log_shutdown(t_log* logger, int reason_shutdown);
static void check_reason_shutdown(int* reason_shutdown, int km_socket);
static void notify_shutdown_kernel_memory(int reason_shutdown, int km_socket,
                                          t_log* logger);

void init_mutex_pid_pcb(void)
{
  pthread_mutex_init(&mutex_pid_pcb, NULL);
}

void init_mutex_shutdown(void)
{
  pthread_mutex_init(&mutex_shutdown, NULL);
}

void destroy_mutex_pid_pcb(void)
{
  pthread_mutex_destroy(&mutex_pid_pcb);
}

void destroy_mutex_shutdown(void)
{
  pthread_mutex_destroy(&mutex_shutdown);
}

t_kernel_memory_socket* init_socket_kernel_memory(int km_socket)
{
  t_kernel_memory_socket* km_socket_mutex =
      malloc(sizeof(t_kernel_memory_socket));
  km_socket_mutex->km_socket = km_socket;
  pthread_mutex_init(&(km_socket_mutex->socket_mutex), NULL);
  return km_socket_mutex;
}

void destroy_kernel_memory(t_kernel_memory_socket* km_socket)
{
  pthread_mutex_destroy(&(km_socket->socket_mutex));
  free(km_socket);
}

int insert_pcb_in_orden(t_list* list, t_pcb* pcb)
{
  return list_add_sorted(list, pcb, is_highest_priority);
}

int get_state_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  int state_pcb = pcb->state;
  pthread_mutex_unlock(&(pcb->state_mutex));
  return state_pcb;
}

int get_priority_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->priority_mutex));
  int priority_pcb = pcb->priority;
  pthread_mutex_unlock(&(pcb->priority_mutex));
  return priority_pcb;
}

t_pcb* create_pcb(int state, int priority)
{
  static uint32_t pid = 0;
  t_pcb* pcb = malloc(sizeof(t_pcb));

  pthread_mutex_init(&(pcb->priority_mutex), NULL);
  pthread_mutex_init(&(pcb->state_mutex), NULL);
  pthread_mutex_init(&(pcb->active_instances_mutex), NULL);
  pthread_cond_init(&(pcb->no_active_instances), NULL);
  pcb->active_instances = 0;
  pcb->blocked_time = 0;
  pcb->state = state;
  pcb->blocking_mutex = NULL;

  pcb->priority_list = list_create();
  pcb->priority = priority;
  int* aux = malloc(sizeof(int));
  *aux = priority;
  list_add(pcb->priority_list, aux);

  pthread_mutex_lock(&mutex_pid_pcb);
  pcb->pid = pid;
  pid++;
  pthread_mutex_unlock(&mutex_pid_pcb);
  return pcb;
}

void incrementar_instances_active_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->active_instances_mutex));
  pcb->active_instances++;
  pthread_mutex_unlock(&(pcb->active_instances_mutex));
}

void disminuir_instances_active_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->active_instances_mutex));
  pcb->active_instances--;
  if (pcb->active_instances == 0)
  {
    pthread_cond_signal(&(pcb->no_active_instances));
  }
  pthread_mutex_unlock(&(pcb->active_instances_mutex));
}

void wait_0_instances_active_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->active_instances_mutex));
  while (pcb->active_instances != 0)
  {
    pthread_cond_wait(&(pcb->no_active_instances),
                      &(pcb->active_instances_mutex));
  }
  pthread_mutex_unlock(&(pcb->active_instances_mutex));
}

void destroy_pcb(t_pcb* pcb)
{
  pthread_mutex_destroy(&(pcb->priority_mutex));
  pthread_mutex_destroy(&(pcb->state_mutex));
  pthread_mutex_destroy(&(pcb->active_instances_mutex));
  pthread_cond_destroy(&(pcb->no_active_instances));
  list_destroy_and_destroy_elements(pcb->priority_list, free);
  free(pcb);
}

void set_mutex_blocking(t_pcb* pcb, void* mutex)
{
  pthread_mutex_lock(&(pcb->priority_mutex));
  pcb->blocking_mutex = mutex;
  pthread_mutex_unlock(&(pcb->priority_mutex));
}

void* get_mutex_blocking(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->priority_mutex));
  void* mutex = pcb->blocking_mutex;
  pthread_mutex_unlock(&(pcb->priority_mutex));
  return mutex;
}

bool respond_handshake(int socket_fd, int id_module, t_log* logger)
{
  if (!send_handshake(id_module, socket_fd))
  {
    log_error(logger, "## Error sending the handshake to %s",
              HANDSHAKE_MSG[id_module]);
    return false;
  }
  return true;
}

unsigned long millis(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

unsigned long time_diff(unsigned long time_1, unsigned long time_2)
{
  return time_1 > time_2 ? time_1 - time_2 : time_2 - time_1;
}

t_process_counter* init_counter_processes(int server_socket, t_log* logger,
                                          t_kernel_memory_socket* km_socket)
{
  t_process_counter* counter = malloc(sizeof(t_process_counter));
  counter->active_process_count = 0;
  pthread_mutex_init(&(counter->counter_mutex), NULL);
  counter->server_socket = server_socket;
  counter->logger = logger;
  counter->km_socket = km_socket;
  return counter;
}

void aumentar_counter_processes(t_process_counter* counter)
{
  pthread_mutex_lock(&(counter->counter_mutex));
  counter->active_process_count++;
  pthread_mutex_unlock(&(counter->counter_mutex));
}

void disminuir_counter_processes(t_process_counter* counter)
{
  pthread_mutex_lock(&(counter->counter_mutex));
  counter->active_process_count--;
  bool is_last = counter->active_process_count == 0;
  pthread_mutex_unlock(&(counter->counter_mutex));

  if (is_last)
  {
    pthread_mutex_lock(&(counter->km_socket->socket_mutex));
    close_kernel_scheduler(counter->server_socket, counter->logger,
                           SR_NO_PROCESSES, counter->km_socket->km_socket);
    pthread_mutex_unlock(&(counter->km_socket->socket_mutex));
  }
}

void destroy_counter_processes(t_process_counter* counter)
{
  pthread_mutex_destroy(&(counter->counter_mutex));
  free(counter);
}

void close_kernel_scheduler(int server_socket, t_log* logger,
                            int reason_shutdown, int km_socket)
{
  static bool shutdown_activado = false;
  pthread_mutex_lock(&mutex_shutdown);
  if (!shutdown_activado)
  {
    notify_shutdown_kernel_memory(reason_shutdown, km_socket, logger);
    check_reason_shutdown(&reason_shutdown, km_socket);
    log_shutdown(logger, reason_shutdown);
    shutdown(server_socket, SHUT_RDWR);
    shutdown_activado = true;
  }
  pthread_mutex_unlock(&mutex_shutdown);
}

static bool is_highest_priority(void* pcb1, void* pcb2)
{
  return get_priority_pcb((t_pcb*)pcb1) <= get_priority_pcb((t_pcb*)pcb2);
}

static void log_shutdown(t_log* logger, int reason_shutdown)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_info(logger, "## %s", SHUTDOWN_REASONS[reason_shutdown]);
  }
  else
  {
    log_error(logger, "## %s", SHUTDOWN_REASONS[reason_shutdown]);
  }
}

static void check_reason_shutdown(int* reason_shutdown, int km_socket)
{
  if (*reason_shutdown != SR_KERNEL_MEMORY_SEND_ERROR)
  {
    return;
  }

  bool keep_running = true;
  while (keep_running)
  {
    switch (receive_op_code(km_socket))
    {
      case OP_CODE_ERROR:
        *reason_shutdown = SR_KERNEL_MEMORY_CONNECTION_FAILURE;
        keep_running = false;
        break;
      case OP_MEMORY_CORRUPTED:
        *reason_shutdown = SR_CORRUPTED_MEMORY;
        keep_running = false;
        break;
      default:
        free(receive_string(km_socket));
        break;
    }
  }
}

static void notify_shutdown_kernel_memory(int reason_shutdown, int km_socket,
                                          t_log* logger)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_info(logger,
             "Notifying Kernel Memory of the Kernel Scheduler shutdown");
    send_string(OP_KERNEL_SCHEDULER_SHUTDOWN, "No more processes to run",
                km_socket);
  }
}
