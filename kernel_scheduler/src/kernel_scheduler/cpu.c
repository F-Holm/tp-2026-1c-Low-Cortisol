#include "kernel_scheduler/cpu.h"

#include <commons/collections/list.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/memory.h"
#include "kernel_scheduler/misc.h"
#include "utils/client.h"
#include "utils/io.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/msg.h"

typedef enum
{
  TS_SYSCALL_MUTEX_CREATE,
  TS_SYSCALL_MUTEX_LOCK,
  TS_SYSCALL_MUTEX_UNLOCK,
  TS_SYSCALL_MEM_ALLOC,
  TS_SYSCALL_MEM_FREE,
  TS_SYSCALL_SLEEP,
  TS_SYSCALL_STDOUT,
  TS_SYSCALL_STDIN,
  TS_SYSCALL_INIT_PROC,
  TS_SYSCALL_EXIT
} t_tipo_syscall;

const char* const SYSCALLS_STR[10] = {
    "MUTEX_CREATE", "MUTEX_LOCK", "MUTEX_UNLOCK", "MEM_ALLOC", "MEM_FREE",
    "SLEEP",        "STDOUT",     "STDIN",        "INIT_PROC", "EXIT"};

static void log_syscall(t_logger* logger, uint32_t pid, int syscall)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## %u - Solicitó syscall: %s", pid,
           SYSCALLS_STR[syscall]);
  pthread_mutex_unlock(&(logger->mutex_logger));
}

static void cerrar_hilo_cpu(t_datos_hilo_cpu* datos)
{
  close(datos->socket_fd);
  pthread_mutex_lock(datos->mutex_lista_sockets);
  list_remove_element(datos->lista_sockets, &(datos->socket_fd));
  if (list_is_empty(datos->lista_sockets))
    pthread_cond_signal(datos->cond_fin);
  pthread_mutex_unlock(datos->mutex_lista_sockets);
  free(datos->id);
  free(datos);
}

static void log_desalojo_fin_quantum(t_logger* logger, uint32_t pid)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## %u - Desalojado por fin de quantum", pid);
  pthread_mutex_unlock(&(logger->mutex_logger));
}

static void log_desalojo_cola_prioritaria(t_logger* logger,
                                          uint32_t pid_desalojado,
                                          int prioridad_desalojado,
                                          uint32_t pid_nuevo,
                                          int prioridad_nuevo)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger,
           "## %u Prioridad: %d - Desalojado por cola más "
           "prioritaria por el proceso %u con prioridad %d",
           pid_desalojado, prioridad_desalojado, pid_nuevo, prioridad_nuevo);
  pthread_mutex_unlock(&(logger->mutex_logger));
}

static void gestionar_cola_bloqueada(t_datos_hilo_cpu* datos, t_pcb** pcb)
{
  if (esta_cola_ready_bloqueada(&(datos->colas->ready)))
  {
    cambio_exec_ready(*pcb, &(datos->colas->exec), &(datos->colas->ready),
                      datos->logger);
    *pcb = NULL;
  }
}

static void gestionar_desalojo(t_datos_hilo_cpu* datos, t_pcb** pcb,
                               int* contador)
{
  if (*pcb != NULL && datos->colas->exec.desalojo)
  {
    int prioridad_desalojado = get_prioridad_pcb(*pcb);
    pthread_mutex_lock(&(datos->colas->ready.mutex_cola));
    int prioridad_nuevo = datos->colas->ready.mayor_prioridad;
    if (prioridad_desalojado > prioridad_nuevo)
    {
      t_pcb* nueva_pcb = cambio_sacar_ready_bloqueante(&(datos->colas->ready));
      pthread_mutex_unlock(&(datos->colas->ready.mutex_cola));

      log_desalojo_cola_prioritaria(datos->logger, (*pcb)->pid,
                                    prioridad_desalojado, nueva_pcb->pid,
                                    prioridad_nuevo);
      cambio_exec_ready(*pcb, &(datos->colas->exec), &(datos->colas->ready),
                        datos->logger);
      *pcb = nueva_pcb;
      cambio_ready_exec(*pcb, &(datos->colas->exec), datos->logger);
      *contador = 0;
    }
    pthread_mutex_unlock(&(datos->colas->ready.mutex_cola));
  }
}

static void gestionar_fin_quantum(t_datos_hilo_cpu* datos, t_pcb** pcb,
                                  int* contador)
{
  if (*pcb != NULL && datos->colas->exec.quantum == *contador)
  {
    log_desalojo_fin_quantum(datos->logger, (*pcb)->pid);
    cambio_exec_ready(*pcb, &(datos->colas->exec), &(datos->colas->ready),
                      datos->logger);
    *pcb = NULL;
  }
}

static void gestionar_pedir_proceso(t_datos_hilo_cpu* datos, t_pcb** pcb,
                                    int* contador)
{
  if (*pcb == NULL)
  {
    *contador = 0;
    *pcb = cambio_sacar_ready_bloqueante(&(datos->colas->ready));
    if (*pcb != NULL)
    {
      cambio_ready_exec(*pcb, &(datos->colas->exec), datos->logger);
    }
  }
}

static void manejar_ciclo_cpu_ok(t_datos_hilo_cpu* datos)
{
  free(recibir_string(datos->socket_fd));
}

static void manejar_syscall_mutex_create(t_datos_hilo_cpu* datos)
{
  char* id_mutex = recibir_string(datos->socket_fd);
  crear_y_add_mutex(datos->lista_mutex, id_mutex,
                    datos->colas->ready.cola_multi_nivel, datos->logger);
  free(id_mutex);
}

static void manejar_syscall_mutex_lock(t_datos_hilo_cpu* datos, t_pcb** pcb)
{
  char* id_mutex = recibir_string(datos->socket_fd);
  if (!lista_mutex_lock(datos->lista_mutex, id_mutex, *pcb))
  {
    *pcb = NULL;
  }
  free(id_mutex);
}

static void manejar_syscall_mutex_unlock(t_datos_hilo_cpu* datos, t_pcb** pcb)
{
  char* id_mutex = recibir_string(datos->socket_fd);
  lista_mutex_unlock(datos->lista_mutex, id_mutex, *pcb);
  free(id_mutex);
}

static void manejar_syscall_memory_allocation(t_datos_hilo_cpu* datos,
                                              bool* seguir_operando)
{
  int size;
  t_syscall_memory* peticion = recibir_buffer(&size, datos->socket_fd);
  if (!allocate_memory(peticion, datos->logger, datos->socket_km))
  {
    *seguir_operando = false;
  }
  free(peticion);
}

static void manejar_syscall_memory_free(t_datos_hilo_cpu* datos,
                                        bool* seguir_operando)
{
  int size;
  t_syscall_memory* peticion = recibir_buffer(&size, datos->socket_fd);
  if (!free_memory(peticion, datos->logger, datos->socket_km))
  {
    *seguir_operando = false;
  }
  free(peticion);
}

static void manejar_syscall_io_sleep(t_datos_hilo_cpu* datos, t_pcb** pcb)
{
  int size;
  t_peticion_sleep* peticion = recibir_buffer(&size, datos->socket_fd);
  if (!procesar_nuevo_sleep(peticion, &(datos->estructuras_io[E_SLEEP]),
                            datos->listas_io->lista_sleep, datos->logger))
  {
    *pcb = NULL;
  }
}

static void manejar_syscall_io_stdout(t_datos_hilo_cpu* datos, t_pcb** pcb)
{
  int size;
  t_peticion_stdout* peticion = recibir_buffer(&size, datos->socket_fd);
  if (!procesar_nuevo_stdout(peticion, &(datos->estructuras_io[E_STDOUT]),
                             datos->listas_io->lista_stdout, datos->logger))
  {
    *pcb = NULL;
  }
}

static void manejar_syscall_io_stdin(t_datos_hilo_cpu* datos, t_pcb** pcb)
{
  int size;
  t_peticion_stdin* peticion = recibir_buffer(&size, datos->socket_fd);
  if (!procesar_nuevo_stdin(peticion, &(datos->estructuras_io[E_STDIN]),
                            datos->listas_io->lista_stdin))
  {
    *pcb = NULL;
  }
}

static void manejar_syscall_iniciar_proceso(t_datos_hilo_cpu* datos)
{
  t_list* lista = recibir_paquete(datos->socket_fd);
  t_pcb* nueva_pcb =
      cambio_sacar_new(list_get(lista, 0), *(int*)list_get(lista, 1),
                       datos->logger, datos->socket_km, datos->socket_servidor);
  list_destroy_and_destroy_elements(lista, free);
  if (nueva_pcb != NULL)
  {
    cambio_new_ready(nueva_pcb, &(datos->colas->ready), datos->logger,
                     datos->colas->contador_procesos);
  }
}

static void manejar_syscall_exit(t_datos_hilo_cpu* datos, t_pcb** pcb)
{
  cambio_exec_exit(*pcb, &(datos->colas->exec), datos->logger,
                   datos->colas->contador_procesos, datos->socket_km,
                   datos->socket_servidor);
  *pcb = NULL;
}

static void manejar_syscall_no_conocida(bool* seguir_operando)
{
  *seguir_operando = false;
}

static void gestionar_op_code(t_datos_hilo_cpu* datos, int op_code, t_pcb** pcb,
                              bool* seguir_operando)
{
  switch (op_code)
  {
    case OP_CICLO_CPU_OK:
      manejar_ciclo_cpu_ok(datos);
      break;
    case OP_SYSCALL_MUTEX_CREATE:
      manejar_syscall_mutex_create(datos);
      break;
    case OP_SYSCALL_MUTEX_LOCK:
      manejar_syscall_mutex_lock(datos, pcb);
      break;
    case OP_SYSCALL_MUTEX_UNLOCK:
      manejar_syscall_mutex_unlock(datos, pcb);
      break;
    case OP_SYSCALL_MEM_ALLOC:
      manejar_syscall_memory_allocation(datos, seguir_operando);
      break;
    case OP_SYSCALL_MEM_FREE:
      manejar_syscall_memory_free(datos, seguir_operando);
      break;
    case OP_SYSCALL_SLEEP:
      manejar_syscall_io_sleep(datos, pcb);
      break;
    case OP_SYSCALL_STDOUT:
      manejar_syscall_io_stdout(datos, pcb);
      break;
    case OP_SYSCALL_STDIN:
      manejar_syscall_io_stdin(datos, pcb);
      break;
    case OP_SYSCALL_INIT_PROC:
      manejar_syscall_iniciar_proceso(datos);
      break;
    case OP_SYSCALL_EXIT:
      manejar_syscall_exit(datos, pcb);
      break;
    default:
      manejar_syscall_no_conocida(seguir_operando);
      break;
  }
}

static void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  t_datos_hilo_cpu* datos = (t_datos_hilo_cpu*)datos_hilo_cpu_void;
  bool seguir_operando = true;
  t_pcb* pcb;
  int contador = 0;

  while (seguir_operando)
  {
    gestionar_cola_bloqueada(datos, &pcb);
    gestionar_desalojo(datos, &pcb, &contador);
    gestionar_fin_quantum(datos, &pcb, &contador);
    gestionar_pedir_proceso(datos, &pcb, &contador);

    if (pcb == NULL || !enviar_buffer(OP_CONTINUAR_PROCESO, &(pcb->pid),
                                      sizeof(uint32_t), datos->socket_fd))
    {
      seguir_operando = false;
      break;
    }

    int op_code = recibir_operacion(datos->socket_fd);
    if (op_code >= OP_SYSCALL_MUTEX_CREATE && op_code <= OP_SYSCALL_EXIT)
    {
      log_syscall(datos->logger, pcb->pid, op_code - OP_SYSCALL_MUTEX_CREATE);
    }

    gestionar_op_code(datos, op_code, &pcb, &seguir_operando);
    contador++;
  }

  if (pcb != NULL)
  {
    cambio_exec_ready(pcb, &(datos->colas->exec), &(datos->colas->ready),
                      datos->logger);
  }
  // Liberar conexión y eliminar socket de la lista
  cerrar_hilo_cpu(datos);
  return NULL;
}

static void iterator_shutdown(void* value)
{
  shutdown(*(int*)value, SHUT_RDWR);
}

static t_datos_hilo_cpu* inicializar_datos_hilo_cpu(
    int socket_cpu, t_list* lista_sockets_cpu,
    pthread_mutex_t* mutex_lista_sockets_cpu, pthread_cond_t* cond_fin_cpu,
    char* id_cpu, t_logger* logger, t_lista_mutex* lista_mutex, t_colas* colas,
    t_io* estructuras_io, t_socket_kernel_memory* socket_km,
    int socket_servidor, t_listas_io* listas_io)
{
  t_datos_hilo_cpu* datos = malloc(sizeof(t_datos_hilo_cpu));
  datos->socket_fd = socket_cpu;
  datos->lista_sockets = lista_sockets_cpu;
  datos->mutex_lista_sockets = mutex_lista_sockets_cpu;
  datos->cond_fin = cond_fin_cpu;
  datos->id = id_cpu;
  datos->logger = logger;
  datos->lista_mutex = lista_mutex;
  datos->colas = colas;
  datos->estructuras_io = estructuras_io;
  datos->socket_km = socket_km;
  datos->socket_servidor = socket_servidor;
  datos->listas_io = listas_io;
  return datos;
}

static bool crear_hilo_cpu(t_datos_hilo_cpu* datos)
{
  pthread_t hilo_cpu;
  if (pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos) != 0)
  {
    pthread_mutex_lock(&(datos->logger->mutex_logger));
    log_error(datos->logger->logger,
              "## Error en la creación del hilo de la CPU");
    pthread_mutex_unlock(&(datos->logger->mutex_logger));
    return false;
  }
  pthread_detach(hilo_cpu);
  return true;
}

void cerrar_cpu(t_list* lista_sockets_cpu,
                pthread_mutex_t* mutex_lista_sockets_cpu,
                pthread_cond_t* cond_fin_cpu)
{
  pthread_mutex_lock(mutex_lista_sockets_cpu);
  list_iterate(lista_sockets_cpu, (void*)iterator_shutdown);
  while (!list_is_empty(lista_sockets_cpu))
    pthread_cond_wait(cond_fin_cpu, mutex_lista_sockets_cpu);
  pthread_mutex_unlock(mutex_lista_sockets_cpu);
  list_destroy(lista_sockets_cpu);
  pthread_cond_destroy(cond_fin_cpu);
  pthread_mutex_destroy(mutex_lista_sockets_cpu);
}

static char* obtener_id_cpu(int socket_cpu, t_logger* logger)
{
  if (recibir_operacion(socket_cpu) != OP_ID_CPU)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## Error en la recepción del ID de la CPU");
    pthread_mutex_unlock(&(logger->mutex_logger));
    return NULL;
  }
  char* id_cpu = recibir_string(socket_cpu);
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## CPU %s Conectada", id_cpu);
  pthread_mutex_unlock(&(logger->mutex_logger));
  return id_cpu;
}

bool atender_nueva_cpu(int socket_cpu, t_list* lista_sockets_cpu,
                       pthread_mutex_t* mutex_lista_sockets_cpu,
                       pthread_cond_t* cond_fin_cpu, t_logger* logger,
                       t_lista_mutex* lista_mutex, t_colas* colas,
                       t_io* estructuras_io, t_socket_kernel_memory* socket_km,
                       int socket_servidor, t_listas_io* listas_io)
{
  // Handshake con CPU
  if (!responder_handshake(socket_cpu, MID_KERNEL_SCHEDULER, logger))
    return false;

  // Obtener ID
  char* id_cpu = obtener_id_cpu(socket_cpu, logger);
  if (id_cpu == NULL)
    return false;

  // Inicializar datos hilo cpu
  t_datos_hilo_cpu* datos_hilo_cpu = inicializar_datos_hilo_cpu(
      socket_cpu, lista_sockets_cpu, mutex_lista_sockets_cpu, cond_fin_cpu,
      id_cpu, logger, lista_mutex, colas, estructuras_io, socket_km,
      socket_servidor, listas_io);

  // Agregar socket a la lista
  pthread_mutex_lock(mutex_lista_sockets_cpu);
  list_add(lista_sockets_cpu, &(datos_hilo_cpu->socket_fd));
  pthread_mutex_unlock(mutex_lista_sockets_cpu);

  // Crear hilo
  if (!crear_hilo_cpu(datos_hilo_cpu))
  {
    pthread_mutex_lock(mutex_lista_sockets_cpu);
    list_remove_element(lista_sockets_cpu, &(datos_hilo_cpu->socket_fd));
    pthread_mutex_unlock(mutex_lista_sockets_cpu);
    free(datos_hilo_cpu->id);
    free(datos_hilo_cpu);
    return false;
  }

  return true;
}
