#include "kernel_scheduler/misc.h"

#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "utils/msg.h"

const char* const ESTADOS_STR[7] = {
    "NEW", "READY", "EXEC", "BLOCK", "SUSP. BLOCK", "SUSP. READY", "EXIT"};

const char* const MOTIVOS_CIERE[4] = {
    "Procesos finalizados con éxito", "BSOD: Corrupción de memoria detectada",
    "Error en la conexión con Kernel Memory", "Error desconocido"};

static pthread_mutex_t mutex_pid_pcb;
static pthread_mutex_t mutex_shutdown;

void inicializar_mutex_pid_pcb(void)
{
  pthread_mutex_init(&mutex_pid_pcb, NULL);
}

void inicializar_mutex_shutdown(void)
{
  pthread_mutex_init(&mutex_shutdown, NULL);
}

void destruir_mutex_pid_pcb(void)
{
  pthread_mutex_destroy(&mutex_pid_pcb);
}

void destruir_mutex_shutdown(void)
{
  pthread_mutex_destroy(&mutex_shutdown);
}

t_socket_kernel_memory* inicializar_socket_kernel_memory(int socket_km)
{
  t_socket_kernel_memory* socket_km_mutex =
      malloc(sizeof(t_socket_kernel_memory));
  socket_km_mutex->socket_km = socket_km;
  pthread_mutex_init(&(socket_km_mutex->mutex_socket), NULL);
  return socket_km_mutex;
}

void destruir_kernel_memory(t_socket_kernel_memory* socket_km)
{
  pthread_mutex_destroy(&(socket_km->mutex_socket));
  free(socket_km);
}

static bool es_mas_prioritario(void* pcb1, void* pcb2)
{
  return get_prioridad_pcb((t_pcb*)pcb1) <= get_prioridad_pcb((t_pcb*)pcb2);
}

int insertar_pcb_en_orden(t_list* lista, t_pcb* pcb)
{
  return list_add_sorted(lista, pcb, es_mas_prioritario);
}

int get_estado_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  int estado_pcb = pcb->estado;
  pthread_mutex_unlock(&(pcb->mutex_estado));
  return estado_pcb;
}

int get_prioridad_pcb(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->mutex_prioridad));
  int prioridad_pcb = pcb->prioridad;
  pthread_mutex_unlock(&(pcb->mutex_prioridad));
  return prioridad_pcb;
}

t_pcb* crear_pcb(void)
{
  static uint32_t pid = 0;
  t_pcb* pcb = malloc(sizeof(t_pcb));

  pthread_mutex_init(&(pcb->mutex_prioridad), NULL);
  pthread_mutex_init(&(pcb->mutex_estado), NULL);
  pcb->tiempo_bloqueado = 0;
  pcb->estado = EST_NEW;
  pcb->lista_prioridades = list_create();

  pthread_mutex_lock(&mutex_pid_pcb);
  pcb->pid = pid;
  pid++;
  pthread_mutex_unlock(&mutex_pid_pcb);
  return pcb;
}

void destruir_pcb(t_pcb* pcb)
{
  pthread_mutex_destroy(&(pcb->mutex_prioridad));
  pthread_mutex_destroy(&(pcb->mutex_estado));
  list_destroy_and_destroy_elements(pcb->lista_prioridades, free);
  free(pcb);
}

bool responder_handshake(int socket_fd, int id_modulo, t_logger* logger)
{
  if (!enviar_handshake(id_modulo, socket_fd))
  {
    logger_error(logger, "## Error en el envio del Handshake con %s",
                 HANDSHAKE_MSG[id_modulo]);
    return false;
  }
  logger_info(logger, "## Handshake exitoso con %s", HANDSHAKE_MSG[id_modulo]);
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
  pthread_mutex_unlock(&(contador->mutex_contador));
}

void disminuir_contador_procesos(t_contador_procesos* contador)
{
  pthread_mutex_lock(&(contador->mutex_contador));
  contador->cantidad_procesos_activos--;
  if (contador->cantidad_procesos_activos == 0)
  {
    cerrar_kernel_scheduler(contador->socket_servidor, contador->logger,
                            MC_SIN_PROCESOS, -1);
  }
  pthread_mutex_unlock(&(contador->mutex_contador));
}

void destruir_contador_procesos(t_contador_procesos* contador)
{
  pthread_mutex_destroy(&(contador->mutex_contador));
  free(contador);
}

static void log_shutdown(t_logger* logger, int motivo_cierre)
{
  if (motivo_cierre == MC_SIN_PROCESOS)
  {
    logger_info(logger, "## %s", MOTIVOS_CIERE[motivo_cierre]);
  }
  else
  {
    logger_error(logger, "## %s", MOTIVOS_CIERE[motivo_cierre]);
  }
}

static void comprobar_motivo_cierre(int* motivo_cierre, int socket_km)
{
  if (*motivo_cierre != MC_ERROR_ENVIO_KERNEL_MEMORY)
  {
    return;
  }

  bool seguir_operando = true;
  while (seguir_operando)
  {
    switch (recibir_operacion(socket_km))
    {
      case OP_CODE_ERROR:
        *motivo_cierre = MC_MEMORIA_CORRUPTA;
        seguir_operando = false;
        break;
      case OP_MEMORIA_CORRUPTA:
        *motivo_cierre = MC_FALLO_CONEXION_KERNEL_MEMORY;
        seguir_operando = false;
        break;
      default:
        free(recibir_string(socket_km));
        break;
    }
  }
}

void cerrar_kernel_scheduler(int socket_servidor, t_logger* logger,
                             int motivo_cierre, int socket_km)
{
  static bool shutdown_activado = false;
  pthread_mutex_lock(&mutex_shutdown);
  if (!shutdown_activado)
  {
    comprobar_motivo_cierre(&motivo_cierre, socket_km);
    log_shutdown(logger, motivo_cierre);
    shutdown(socket_servidor, SHUT_RDWR);
    shutdown_activado = true;
  }
  pthread_mutex_unlock(&mutex_shutdown);
}
