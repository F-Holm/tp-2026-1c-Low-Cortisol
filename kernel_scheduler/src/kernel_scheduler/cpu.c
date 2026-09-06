#include "kernel_scheduler/cpu.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/memory.h"
#include "kernel_scheduler/misc.h"
#include "misc.h"
#include "utils/collections/list.h"
#include "utils/io.h"
#include "utils/syscalls.h"
#include "utils/msg.h"

const char* const MOTIVOS_DESALOJO[13] = {
    "no hubo desalojo",
    "desalojo por fin de quantum",
    "desalojo por proceso prioritario",
    "desalojo por compactación",
    "finalización del proceso",
    "primer ciclo de CPU",
    "operación de IO",
    "mutex bloqueado",
    "no hay memoria suficiente para esa instrucción",
    "segmentation fault",
    "ya existe un mutex con ese nombre",
    "no existe un mutex con ese nombre",
    "este proceso no puede desbloquear este mutex"};

const char* const SYSCALLS_STR[10] = {
    "MUTEX_CREATE", "MUTEX_LOCK", "MUTEX_UNLOCK", "MEM_ALLOC", "MEM_FREE",
    "SLEEP",        "STDOUT",     "STDIN",        "INIT_PROC", "EXIT"};

static void log_syscall(t_datos_syscall* datos, int op_code);
static void cerrar_hilo_cpu(t_datos_hilo_cpu* datos);
static void log_desalojo_cola_prioritaria(t_log* logger,
                                          uint32_t pid_desalojado,
                                          int prioridad_desalojado,
                                          uint32_t pid_nuevo,
                                          int prioridad_nuevo);
static void gestionar_cola_bloqueada(t_datos_syscall* datos);
static void log_desalojo_fin_quantum(t_log* logger, uint32_t pid);
static bool es_proceso_menos_prioritario(t_datos_syscall* datos, int prioridad);
static void gestionar_desalojo_prioritario(t_datos_syscall* datos);
static void gestionar_fin_quantum(t_datos_syscall* datos);
static bool enviar_desalojo(t_datos_syscall* datos);
static void gestionar_pedir_proceso(t_datos_syscall* datos);
static bool enviar_codigo(t_datos_syscall* datos);
static void manejar_ciclo_cpu_ok(t_datos_syscall* datos);
static void manejar_segmentation_fault(t_datos_syscall* datos);
static void manejar_syscall_mutex_create(t_datos_syscall* datos);
static void manejar_syscall_mutex_lock(t_datos_syscall* datos);
static void manejar_syscall_mutex_unlock(t_datos_syscall* datos);
static void manejar_syscall_memory_allocation(t_datos_syscall* datos);
static void manejar_syscall_memory_free(t_datos_syscall* datos);
static void manejar_syscall_io_sleep(t_datos_syscall* datos);
static void manejar_syscall_io_stdout(t_datos_syscall* datos);
static void manejar_syscall_io_stdin(t_datos_syscall* datos);
static void manejar_syscall_iniciar_proceso(t_datos_syscall* datos);
static void manejar_syscall_exit(t_datos_syscall* datos);
static void manejar_syscall_no_valida(t_datos_syscall* datos);
static bool enviar_pid(t_datos_syscall* datos);
static void* manejar_cliente_cpu(void* datos_hilo_cpu_void);
static void iterator_shutdown(void* value);
static t_datos_hilo_cpu* inicializar_datos_hilo_cpu(
    int socket_cpu, t_list* lista_sockets_cpu,
    pthread_mutex_t* mutex_lista_sockets_cpu, pthread_cond_t* cond_fin_cpu,
    char* id_cpu, t_log* logger, t_lista_mutex* lista_mutex, t_colas* colas,
    t_io* estructuras_io, t_socket_kernel_memory* socket_km,
    int socket_servidor);
static bool crear_hilo_cpu(t_datos_hilo_cpu* datos);
static char* obtener_id_cpu(int socket_cpu, t_log* logger);

bool atender_nueva_cpu(int socket_cpu, t_list* lista_sockets_cpu,
                       pthread_mutex_t* mutex_lista_sockets_cpu,
                       pthread_cond_t* cond_fin_cpu, t_log* logger,
                       t_lista_mutex* lista_mutex, t_colas* colas,
                       t_io* estructuras_io, t_socket_kernel_memory* socket_km,
                       int socket_servidor)
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
      socket_servidor);

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

void cerrar_cpu(t_list* lista_sockets_cpu,
                pthread_mutex_t* mutex_lista_sockets_cpu,
                pthread_cond_t* cond_fin_cpu, t_colas* colas)
{
  terminar_cola_ready(&(colas->ready));
  pthread_mutex_lock(mutex_lista_sockets_cpu);
  list_iterate(lista_sockets_cpu, (void*)iterator_shutdown);
  while (!list_is_empty(lista_sockets_cpu))
    pthread_cond_wait(cond_fin_cpu, mutex_lista_sockets_cpu);
  pthread_mutex_unlock(mutex_lista_sockets_cpu);
  list_destroy(lista_sockets_cpu);
  pthread_cond_destroy(cond_fin_cpu);
  pthread_mutex_destroy(mutex_lista_sockets_cpu);
}

static void log_syscall(t_datos_syscall* datos, int op_code)
{
  if (op_code >= OP_SYSCALL_MUTEX_CREATE && op_code <= OP_SYSCALL_EXIT)
  {
    log_info(datos->datos->logger, "## %u - Solicitó syscall: %s",
             datos->pcb->pid, SYSCALLS_STR[op_code - OP_SYSCALL_MUTEX_CREATE]);
  }
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

static void log_desalojo_cola_prioritaria(t_log* logger,
                                          uint32_t pid_desalojado,
                                          int prioridad_desalojado,
                                          uint32_t pid_nuevo,
                                          int prioridad_nuevo)
{
  log_info(logger,
           "## %u Prioridad: %d - Desalojado por cola más "
           "prioritaria por el proceso %u con prioridad %d",
           pid_desalojado, prioridad_desalojado, pid_nuevo, prioridad_nuevo);
}

static void gestionar_cola_bloqueada(t_datos_syscall* datos)
{
  if (datos->motivo_desalojo == MD_SIN_DESALOJO &&
      esta_cola_ready_bloqueada(&(datos->datos->colas->ready)))
  {
    log_info(datos->datos->logger, "CPU %s: Ejecución de procesos bloqueada",
             datos->datos->id);
    cambio_exec_ready(datos->pcb, datos->datos->colas);
    datos->motivo_desalojo = MD_COMPACTACION;
  }
}

static void log_desalojo_fin_quantum(t_log* logger, uint32_t pid)
{
  log_info(logger, "## %u - Desalojado por fin de quantum", pid);
}

static bool es_proceso_menos_prioritario(t_datos_syscall* datos, int prioridad)
{
  pthread_mutex_lock(&(datos->datos->colas->exec.mutex_lista));
  bool ret = prioridad >= datos->datos->colas->exec.prioridad_mas_baja;
  pthread_mutex_unlock(&(datos->datos->colas->exec.mutex_lista));
  return ret;
}

static void gestionar_desalojo_prioritario(t_datos_syscall* datos)
{
  if (datos->motivo_desalojo != MD_SIN_DESALOJO ||
      !datos->datos->colas->exec.desalojo || datos->pcb == NULL)
  {
    return;
  }

  int prioridad_desalojado = get_prioridad_pcb(datos->pcb);

  if (!es_proceso_menos_prioritario(datos, prioridad_desalojado))
  {
    return;
  }

  pthread_mutex_lock(&(datos->datos->colas->ready.mutex_cola));
  int prioridad_nuevo = datos->datos->colas->ready.mayor_prioridad;

  if (prioridad_desalojado <= prioridad_nuevo)
  {
    pthread_mutex_unlock(&(datos->datos->colas->ready.mutex_cola));
    return;
  }

  log_info(datos->datos->logger,
           "CPU %s: Desalojando proceso por cola prioritaria",
           datos->datos->id);
  t_pcb* nueva_pcb =
      cambio_sacar_ready_siguiente_sin_mutex(&(datos->datos->colas->ready));
  pthread_mutex_unlock(&(datos->datos->colas->ready.mutex_cola));

  if (nueva_pcb == NULL)
  {
    log_error(datos->datos->logger, "Error en el desalojo por prioridad");
    return;
  }

  log_desalojo_cola_prioritaria(datos->datos->logger, datos->pcb->pid,
                                prioridad_desalojado, nueva_pcb->pid,
                                prioridad_nuevo);
  cambio_exec_ready(datos->pcb, datos->datos->colas);
  cambio_ready_exec(nueva_pcb, datos->datos->colas);
  cambio_a_exec(nueva_pcb, &(datos->datos->colas->exec));
  datos->motivo_desalojo = MD_PROCESO_PRIORITARIO;
  datos->pcb = nueva_pcb;
  datos->contador = millis();
}

static bool es_rr(t_datos_syscall* datos)
{
  if (!datos->datos->colas->ready.cola_multi_nivel)
  {
    return datos->datos->colas->ready.colas->algoritmo == AP_RR;
  }
  return datos->datos->colas->ready.colas[get_prioridad_pcb(datos->pcb)]
             .algoritmo == AP_RR;
}

static void gestionar_fin_quantum(t_datos_syscall* datos)
{
  if (datos->motivo_desalojo == MD_SIN_DESALOJO &&
      !esta_cola_ready_vacia(&(datos->datos->colas->ready)) && es_rr(datos) &&
      datos->datos->colas->exec.quantum <= time_diff(datos->contador, millis()))
  {
    log_info(datos->datos->logger, "CPU %s: Desalojando por fin de quantum",
             datos->datos->id);
    log_desalojo_fin_quantum(datos->datos->logger, datos->pcb->pid);
    cambio_exec_ready(datos->pcb, datos->datos->colas);
    datos->motivo_desalojo = MD_FIN_QUANTUM;
  }
}

static bool enviar_desalojo(t_datos_syscall* datos)
{
  log_info(datos->datos->logger, "CPU %s: Enviando mensaje de desalojo: %s",
           datos->datos->id, MOTIVOS_DESALOJO[datos->motivo_desalojo]);
  return enviar_string(
      (datos->motivo_desalojo != MD_SIN_DESALOJO ? OP_INTERRUPCION
                                                 : OP_SIN_INTERRUPCION),
      (char*)MOTIVOS_DESALOJO[datos->motivo_desalojo], datos->datos->socket_fd);
}

static void gestionar_pedir_proceso(t_datos_syscall* datos)
{
  if (datos->motivo_desalojo != MD_SIN_DESALOJO)
  {
    log_info(datos->datos->logger, "CPU %s: Pidiendo nuevo proceso",
             datos->datos->id);
    datos->contador = millis();
    datos->pcb = cambio_sacar_ready_bloqueante(&(datos->datos->colas->ready));
    if (datos->pcb != NULL)
    {
      log_info(datos->datos->logger, "CPU %s: obtuvo proceso: %u",
               datos->datos->id, datos->pcb->pid);
      cambio_ready_exec(datos->pcb, datos->datos->colas);
      cambio_a_exec(datos->pcb, &(datos->datos->colas->exec));
    }
  }
}

static bool enviar_codigo(t_datos_syscall* datos)
{
  return enviar_buffer(OP_CONTINUAR_PROCESO, &(datos->pcb->pid),
                       sizeof(uint32_t), datos->datos->socket_fd);
}

static void manejar_ciclo_cpu_ok(t_datos_syscall* datos)
{
  free(recibir_string(datos->datos->socket_fd));
  log_info(datos->datos->logger, "CPU %s: Ciclo CPU OK", datos->datos->id);
}

static void manejar_segmentation_fault(t_datos_syscall* datos)
{
  free(recibir_string(datos->datos->socket_fd));
  cambio_exec_exit(datos->pcb, datos->datos->colas, MPF_SEGMENTATION_FAULT);
  datos->motivo_desalojo = MD_SEGMENTATION_FAULT;
}

static void manejar_syscall_mutex_create(t_datos_syscall* datos)
{
  char* id_mutex = recibir_string(datos->datos->socket_fd);
  switch (crear_y_add_mutex(datos->datos->lista_mutex, id_mutex, true,
                            datos->datos->colas))
  {
    case RM_MUTEX_CREADO:
      break;
    case RM_NOMBRE_MUTEX_YA_EXISTE:
      cambio_exec_exit(datos->pcb, datos->datos->colas,
                       MPF_NOMBRE_MUTEX_YA_EXISTE);
      datos->motivo_desalojo = MD_NOMBRE_MUTEX_YA_EXISTE;
      break;
  }
  free(id_mutex);
}

static void manejar_syscall_mutex_lock(t_datos_syscall* datos)
{
  char* id_mutex = recibir_string(datos->datos->socket_fd);
  switch (lista_mutex_lock(datos->datos->lista_mutex, id_mutex, datos->pcb))
  {
    case RM_NOMBRE_MUTEX_NO_EXISTE:
      cambio_exec_exit(datos->pcb, datos->datos->colas,
                       MPF_NOMBRE_MUTEX_NO_EXISTE);
      datos->motivo_desalojo = MD_NOMBRE_MUTEX_NO_EXISTE;
      break;
    case RM_MUTEX_BLOQUEADO:
      break;
    case RM_ESPERANDO_MUTEX:
      datos->motivo_desalojo = MD_MUTEX_BLOQUEADO;
      break;
  }
  free(id_mutex);
}

static void manejar_syscall_mutex_unlock(t_datos_syscall* datos)
{
  char* id_mutex = recibir_string(datos->datos->socket_fd);
  switch (lista_mutex_unlock(datos->datos->lista_mutex, id_mutex, datos->pcb))
  {
    case RM_NOMBRE_MUTEX_NO_EXISTE:
      cambio_exec_exit(datos->pcb, datos->datos->colas,
                       MPF_NOMBRE_MUTEX_NO_EXISTE);
      datos->motivo_desalojo = MD_NOMBRE_MUTEX_NO_EXISTE;
      break;
    case RM_MUTEX_DESBLOQUEADO:
      break;
    case RM_PROCESO_NO_TIENE_MUTEX_BLOQUEADO:
      cambio_exec_exit(datos->pcb, datos->datos->colas,
                       MPF_PROCESO_NO_TIENE_MUTEX_BLOQUEADO);
      datos->motivo_desalojo = MD_PROCESO_NO_TIENE_MUTEX_BLOQUEADO;
      break;
  }
  free(id_mutex);
}

static void manejar_syscall_memory_allocation(t_datos_syscall* datos)
{
  int size;
  t_syscall_memory* peticion = recibir_buffer(&size, datos->datos->socket_fd);
  if (!allocate_memory(peticion, datos->datos->colas))
  {
    cambio_exec_exit(datos->pcb, datos->datos->colas, MPF_MEMORIA_INSUFICIENTE);
    datos->motivo_desalojo = MD_MEMORIA_INSUFICIENTE;
  }
  free(peticion);
}

static void manejar_syscall_memory_free(t_datos_syscall* datos)
{
  int size;
  t_syscall_memory* peticion = recibir_buffer(&size, datos->datos->socket_fd);
  if (!free_memory(peticion, datos->datos->colas))
  {
    datos->seguir_operando = false;
  }
  free(peticion);
}

static void manejar_syscall_io_sleep(t_datos_syscall* datos)
{
  int size;
  t_peticion_sleep* peticion = recibir_buffer(&size, datos->datos->socket_fd);
  datos->motivo_desalojo = MD_IO;
  if (!procesar_nuevo_io(peticion, &(datos->datos->estructuras_io[E_SLEEP]),
                         datos->pcb))
  {
    cambio_exec_exit(datos->pcb, datos->datos->colas, MFP_FALLO_IO);
  }
  else
  {
    cambio_exec_block(datos->pcb, datos->datos->colas);
  }
}

static void manejar_syscall_io_stdout(t_datos_syscall* datos)
{
  int size;
  t_peticion_stdout* peticion = recibir_buffer(&size, datos->datos->socket_fd);
  datos->motivo_desalojo = MD_IO;
  if (!procesar_nuevo_io(peticion, &(datos->datos->estructuras_io[E_STDOUT]),
                         datos->pcb))
  {
    cambio_exec_exit(datos->pcb, datos->datos->colas, MFP_FALLO_IO);
  }
  else
  {
    cambio_exec_block(datos->pcb, datos->datos->colas);
  }
}

static void manejar_syscall_io_stdin(t_datos_syscall* datos)
{
  int size;
  t_peticion_stdin* peticion = recibir_buffer(&size, datos->datos->socket_fd);
  datos->motivo_desalojo = MD_IO;
  if (!procesar_nuevo_io(peticion, &(datos->datos->estructuras_io[E_STDIN]),
                         datos->pcb))
  {
    cambio_exec_exit(datos->pcb, datos->datos->colas, MFP_FALLO_IO);
  }
  else
  {
    cambio_exec_block(datos->pcb, datos->datos->colas);
  }
}

static void manejar_syscall_iniciar_proceso(t_datos_syscall* datos)
{
  t_list* lista = recibir_paquete(datos->datos->socket_fd);
  cambio_new_ready(datos->datos->colas, list_get(lista, 0),
                   *(int*)list_get(lista, 1));
  list_destroy_and_destroy_elements(lista, free);
}

static void manejar_syscall_exit(t_datos_syscall* datos)
{
  free(recibir_string(datos->datos->socket_fd));
  cambio_exec_exit(datos->pcb, datos->datos->colas, MFP_INSTRUCCION_EXIT);
  datos->motivo_desalojo = MD_FIN_PROCESO;
}

static void manejar_syscall_no_valida(t_datos_syscall* datos)
{
  datos->seguir_operando = false;
}

static bool enviar_pid(t_datos_syscall* datos)
{
  if (datos->motivo_desalojo == MD_SIN_DESALOJO)
  {
    return true;
  }

  if (!enviar_codigo(datos))
  {
    log_info(datos->datos->logger, "CPU %s: Fallo al enviar el código",
             datos->datos->id);
    datos->seguir_operando = false;
    return false;
  }

  log_info(datos->datos->logger, "CPU %s: Enviar el código exitoso",
           datos->datos->id);
  datos->motivo_desalojo = MD_SIN_DESALOJO;

  return true;
}

static void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  t_datos_syscall datos_syscall = {(t_datos_hilo_cpu*)datos_hilo_cpu_void, NULL,
                                   true, 0, MD_PRIMER_CICLO};
  void (*funciones_syscalls[OP_SYSCALL_EXIT - OP_CICLO_CPU_OK + 2])(
      t_datos_syscall*) = {manejar_ciclo_cpu_ok,
                           manejar_segmentation_fault,
                           manejar_syscall_mutex_create,
                           manejar_syscall_mutex_lock,
                           manejar_syscall_mutex_unlock,
                           manejar_syscall_memory_allocation,
                           manejar_syscall_memory_free,
                           manejar_syscall_io_sleep,
                           manejar_syscall_io_stdout,
                           manejar_syscall_io_stdin,
                           manejar_syscall_iniciar_proceso,
                           manejar_syscall_exit,
                           manejar_syscall_no_valida};

  while (datos_syscall.seguir_operando)
  {
    gestionar_pedir_proceso(&datos_syscall);

    if (datos_syscall.pcb == NULL)
    {
      log_info(datos_syscall.datos->logger, "CPU %s: Cierre de CPU",
               datos_syscall.datos->id);
      datos_syscall.seguir_operando = false;
      break;
    }

    if (!enviar_pid(&datos_syscall))
    {
      break;
    }

    int op_code = recibir_operacion(datos_syscall.datos->socket_fd);
    log_info(datos_syscall.datos->logger, "CPU %s: Operación recibida: %d",
             datos_syscall.datos->id, op_code);
    if (op_code < OP_CICLO_CPU_OK || op_code > OP_SYSCALL_EXIT)
    {
      op_code = OP_SYSCALL_EXIT + 1;
    }

    if (op_code >= OP_SYSCALL_MUTEX_CREATE && op_code <= OP_SYSCALL_EXIT)
    {
      sumar_contador_syscalls(datos_syscall.datos->colas);
    }
    log_syscall(&datos_syscall, op_code);
    funciones_syscalls[op_code - OP_CICLO_CPU_OK](&datos_syscall);
    if (op_code >= OP_SYSCALL_MUTEX_CREATE && op_code <= OP_SYSCALL_EXIT)
    {
      restar_contador_syscalls(datos_syscall.datos->colas);
    }

    gestionar_cola_bloqueada(&datos_syscall);
    gestionar_fin_quantum(&datos_syscall);
    gestionar_desalojo_prioritario(&datos_syscall);

    if (!enviar_desalojo(&datos_syscall))
    {
      log_info(datos_syscall.datos->logger,
               "CPU %s: Error al enviar el desalojo", datos_syscall.datos->id);
      datos_syscall.seguir_operando = false;
      break;
    }

    if (datos_syscall.motivo_desalojo == MD_PROCESO_PRIORITARIO)
    {
      log_info(datos_syscall.datos->logger,
               "CPU %s: Actualizando motivo desalojo (proceso prioritario)",
               datos_syscall.datos->id);
      if (!enviar_pid(&datos_syscall))
      {
        break;
      }
    }
  }

  log_info(datos_syscall.datos->logger, "CPU %s: Cerrando hilo",
           datos_syscall.datos->id);
  if (datos_syscall.motivo_desalojo == MD_PROCESO_PRIORITARIO &&
      datos_syscall.pcb != NULL)
  {
    log_info(datos_syscall.datos->logger,
             "CPU %s: Guardando proceso en ejecución", datos_syscall.datos->id);
    cambio_exec_ready(datos_syscall.pcb, datos_syscall.datos->colas);
  }
  // Liberar conexión y eliminar socket de la lista
  cerrar_hilo_cpu(datos_syscall.datos);
  return NULL;
}

static void iterator_shutdown(void* value)
{
  shutdown(*(int*)value, SHUT_RDWR);
}

static t_datos_hilo_cpu* inicializar_datos_hilo_cpu(
    int socket_cpu, t_list* lista_sockets_cpu,
    pthread_mutex_t* mutex_lista_sockets_cpu, pthread_cond_t* cond_fin_cpu,
    char* id_cpu, t_log* logger, t_lista_mutex* lista_mutex, t_colas* colas,
    t_io* estructuras_io, t_socket_kernel_memory* socket_km,
    int socket_servidor)
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
  return datos;
}

static bool crear_hilo_cpu(t_datos_hilo_cpu* datos)
{
  pthread_t hilo_cpu;
  if (pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos) != 0)
  {
    log_error(datos->logger, "Error en la creación del hilo de la CPU");
    return false;
  }
  pthread_detach(hilo_cpu);
  return true;
}

static char* obtener_id_cpu(int socket_cpu, t_log* logger)
{
  if (recibir_operacion(socket_cpu) != OP_ID_CPU)
  {
    log_error(logger, "Error en la recepción del ID de la CPU");
    return NULL;
  }
  char* id_cpu = recibir_string(socket_cpu);
  log_info(logger, "CPU %s Conectada", id_cpu);
  return id_cpu;
}
