#include "kernel_scheduler/misc.h"

#include <pthread.h>
#include <sys/time.h>

#include "utils/msg.h"

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

bool responder_handshake(int socket_fd, int id_modulo, t_log* logger)
{
  if (!enviar_handshake(id_modulo, socket_fd))
  {
    log_error(logger, "## Error en el envio del Handshake con %s",
              HANDSHAKE_MSG[id_modulo]);
    return false;
  }
  log_info(logger, "## Handshake exitoso con %s", HANDSHAKE_MSG[id_modulo]);
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
