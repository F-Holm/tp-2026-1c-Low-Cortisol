#include "kernel_scheduler/misc.h"

#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "utils/msg.h"

t_socket_kernel_memory* inicializar_socket_kernel_memory(int socket_km)
{
  t_socket_kernel_memory* socket_km_mutex =
      malloc(sizeof(t_socket_kernel_memory));
  socket_km_mutex->socket_km = socket_km;
  pthread_mutex_init(&(socket_km->mutex_socket), NULL);
}

void destruir_kernel_memory(t_socket_kernel_memory* socket_km)
{
  pthread_mutex_destroy(&(socket_km->mutex_socket));
  free(socket_km);
}

static bool es_mas_prioritario(t_pcb* pcb1, t_pcb* pcb2)
{
  return get_prioridad_pcb(pcb1) <= get_prioridad_pcb(pcb2);
}

void insertar_pcb_en_orden(t_list* lista, t_pcb* pcb)
{
  list_add_sorted(lista, pcb, es_mas_prioritario);
}

int get_prioridad_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->mutex_pcb));
  int prioridad_pcb = pcb->prioridad;
  pthread_mutex_unlock(&(pcb->mutex_pcb));
  return prioridad_pcb;
}

t_pcb* crear_pcb(void)
{
  static uint32_t pid = 0;
  t_pcb* pcb = malloc(sizeof(t_pcb));

  pcb->pid = pid;
  pthread_mutex_init(&(pcb->mutex_pcb));
  pcb->tiempo_bloqueado = 0;

  pid++;
}

void destruir_pcb(t_pcb* pcb)
{
  pthread_mutex_destroy(&(pcb->mutex_pcb));
  free(pcb);
}

bool responder_handshake(int socket_fd, int id_modulo, t_logger* logger)
{
  if (!enviar_handshake(id_modulo, socket_fd))
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger, "## Error en el envio del Handshake con %s",
              HANDSHAKE_MSG[id_modulo]);
    pthread_mutex_unlock(&(logger->mutex_logger));
    return false;
  }
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## Handshake exitoso con %s",
           HANDSHAKE_MSG[id_modulo]);
  pthread_mutex_unlock(&(logger->mutex_logger));
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

t_contador_procesos* inicializar_contador_procesos(int socket_servidor,
                                                   t_logger* logger)
{
  t_contador_procesos* contador = malloc(sizeof(t_contador_procesos));
  contador->cantidad_procesos_activos = 0;
  pthread_mutex_init(&(contador->mutex_contador), NULL);
  contador->socket_servidor = socket_servidor;
  contador->logger = logger;
  return contador;
}

void aumentar_contador_procesos(t_contador_procesos* contador)
{
  pthread_mutex_lock(&(contador->mutex_contador));
  contador->cantidad_procesos_activos++;
  pthread_mutex_lock(&(contador->mutex_contador));
}

void disminuir_contador_procesos(t_contador_procesos* contador)
{
  pthread_mutex_lock(&(contador->mutex_contador));
  contador->cantidad_procesos_activos--;
  if (contador->cantidad_procesos_activos == 0)
  {
    cerrar_kernel_scheduler(contador->socket_servidor, contador->logger,
                            MC_SIN_PROCESOS)
  }
  pthread_mutex_lock(&(contador->mutex_contador));
}

void destruir_contador_procesos(t_contador_procesos* contador)
{
  pthread_mutex_destroy(&(contador->mutex_contador));
  free(contador);
}

static void log_shutdown(t_logger* logger, int motivo_cierre)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  switch (motivo_cierre)
  {
    case MC_SIN_PROCESOS:
      log_info(logger->logger, "## Procesos finalizados con éxito");
      break;
    case MC_MEMORIA_CORRUPTA:
      log_error(logger->logger, "## BSOD: Corrupción de memoria detectada");
      break;
    case MC_FALLO_CONEXION_KERNEL_MEMORY:
      log_error(logger->logger, "## Error en la conexión con kernel");
      break;
    default:
      log_error(logger->logger, "## Error desconocido");
      break;
  }
  pthread_mutex_unlock(&(logger->mutex_logger));
}

void cerrar_kernel_scheduler(int socket_servidor, t_logger* logger,
                             int motivo_cierre)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  static bool shutdown_activado = false;
  pthread_mutex_unlock(&(logger->mutex_logger));
  if (!shutdown_activado)
  {
    log_shutdown(logger, motivo_cierre);
    shutdown(socket_servidor, SHUT_RDWR);
    shutdown_activado = true;
  }
}
