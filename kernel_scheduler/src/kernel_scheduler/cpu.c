#include "kernel_scheduler/cpu.h"

#include <commons/collections/list.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/misc.h"
#include "utils/client.h"
#include "utils/io.h"
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
  log_info(logger->logger, "## %u - Desalojado por fin de quantum", pid);
}

static void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  t_datos_hilo_cpu* datos = (t_datos_hilo_cpu*)datos_hilo_cpu_void;
  bool seguir_operando = true;
  t_pcb* pcb;
  int contador = 0;

  while (seguir_operando)
  {
    if (pcb != NULL)
    {
      if (datos->colas.exec.quantum == contador)
      {
        log_desalojo_fin_quantum(datos->logger, pcb->pid);
        cambio_exec_ready(pcb, &(datos->colas.exec), &(datos->colas.ready),
                          datos->logger);
        pcb = NULL;
        contador = 0;
      }
    }

    if (pcb == NULL)
    {
      do
      {
        pcb = cambio_sacar_ready(&(datos->colas.ready));
      } while (pcb == NULL);
      cambio_a_exec(pcb, &(datos->colas.exec));
    }
    // check desalojo
    // check bloquear
    // obtener proceso // wait proceso
    // enviar pid
    // obtener codigo operacion
    // obtener respuesta

    int op_code = recibir_operacion(datos->socket_fd);
    switch (op_code)
    {
      case OP_CICLO_CPU_OK:
        free(recibir_string(datos->socket_fd));
        break;
      case OP_SYSCALL_MUTEX_CREATE:
        char* id_mutex = recibir_string(datos->socket_fd);
        crear_y_add_mutex(datos->lista_mutex, id_mutex,
                          datos->colas->ready.cola_multi_nivel, datos->logger);
        free(id_mutex);
        break;
      case OP_SYSCALL_MUTEX_LOCK:
        char* id_mutex = recibir_string(datos->socket_fd);
        if (!lista_mutex_lock(datos->lista_mutex, id_mutex, pcb))
        {
          pcb = NULL;
        }
        break;
      case OP_SYSCALL_MUTEX_UNLOCK:
        char* id_mutex = recibir_string(datos->socket_fd);
        lista_mutex_unlock(datos->lista_mutex, id_mutex, pcb);
        break;
      case OP_SYSCALL_MEM_ALLOC:
        break;
      case OP_SYSCALL_MEM_FREE:
        break;
      case OP_SYSCALL_SLEEP:
        break;
      case OP_SYSCALL_STDOUT:
        break;
      case OP_SYSCALL_STDIN:
        break;
      case OP_SYSCALL_INIT_PROC:
        break;
      case OP_SYSCALL_EXIT:
        cambio_exec_exit(pcb, datos->colas.exec, datos->logger);
        pcb = NULL;
        break;
      default:
        seguir_operando = false;
        break;
    }
    contador++;
  }

  if (pcb != NULL)
  {
    cambio_exec_ready(pcb, &(datos->colas.exec), &(datos->colas.ready),
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
    t_io* estructuras_io)
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

static char* obtener_id_cpu(int socket_cpu, t_log* logger)
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
                       t_io* estructuras_io)
{
  // Handshake con CPU
  if (!responder_handshake(socket_cpu, MID_KERNEL_SCHEDULER, datos->logger))
    return false;

  // Obtener ID
  char* id_cpu = obtener_id_cpu(socket_cpu, datos->logger);
  if (id_cpu == NULL)
    return false;

  // Inicializar datos hilo cpu
  t_datos_hilo_cpu* datos_hilo_cpu = inicializar_datos_hilo_cpu(
      socket_cpu, lista_sockets_cpu, mutex_lista_sockets_cpu, cond_fin_cpu,
      id_cpu, logger, lista_mutex, colas, estructuras_io);

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
