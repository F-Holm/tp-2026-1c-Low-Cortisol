#include "kernel_scheduler/cpu.h"

#include <commons/collections/list.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/misc.h"
#include "utils/client.h"
#include "utils/io.h"
#include "utils/msg.h"

void cerrar_hilo_cpu(t_datos_hilo_cpu* datos)
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

void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  t_datos_hilo_cpu* datos = (t_datos_hilo_cpu*)datos_hilo_cpu_void;

  while (true)
  {
    int operacion = recibir_operacion(datos->socket_fd);
    if (operacion == OP_CODE_ERROR)
      break;
    char* buffer = recibir_string(datos->socket_fd);
    free(buffer);
  }

  // Liberar conexión y eliminar socket de la lista
  cerrar_hilo_cpu(datos);
  return NULL;
}

void iterator_shutdown(void* value)
{
  shutdown(*(int*)value, SHUT_RDWR);
}

t_datos_hilo_cpu* inicializar_datos_hilo_cpu(
    int socket_cpu, t_list* lista_sockets_cpu,
    pthread_mutex_t* mutex_lista_sockets_cpu, pthread_cond_t* cond_fin_cpu,
    char* id_cpu)
{
  t_datos_hilo_cpu* datos = malloc(sizeof(t_datos_hilo_cpu));
  datos->socket_fd = socket_cpu;
  datos->lista_sockets = lista_sockets_cpu;
  datos->mutex_lista_sockets = mutex_lista_sockets_cpu;
  datos->cond_fin = cond_fin_cpu;
  datos->id = id_cpu;
  return datos;
}

bool crear_hilo_cpu(t_datos_hilo_cpu* datos, t_log* logger)
{
  pthread_t hilo_cpu;
  if (pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos) != 0)
  {
    log_error(logger, "## Error en la creación del hilo de la CPU");
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

char* obtener_id_cpu(int socket_cpu, t_log* logger)
{
  if (recibir_operacion(socket_cpu) != OP_ID_CPU)
  {
    log_error(logger, "## Error en la recepción del ID de la CPU");
    return NULL;
  }
  char* id_cpu = recibir_string(socket_cpu);
  log_info(logger, "## CPU %s Conectada", id_cpu);
  return id_cpu;
}

bool atender_nueva_cpu(t_datos_hilo_escucha* datos, int socket_cpu,
                       t_list* lista_sockets_cpu,
                       pthread_mutex_t* mutex_lista_sockets_cpu,
                       pthread_cond_t* cond_fin_cpu)
{
  // Handshake con CPU
  if (!responder_handshake(socket_cpu, MID_KERNEL_SCHEDULER, datos->logger))
    return false;

  // Obtener ID
  char* id_cpu = obtener_id_cpu(socket_cpu, datos->logger);
  if (id_cpu == NULL)
    return false;

  // Inicializar datos hilo cpu
  t_datos_hilo_cpu* datos_hilo_cpu =
      inicializar_datos_hilo_cpu(socket_cpu, lista_sockets_cpu,
                                 mutex_lista_sockets_cpu, cond_fin_cpu, id_cpu);

  // Agregar socket a la lista
  pthread_mutex_lock(mutex_lista_sockets_cpu);
  list_add(lista_sockets_cpu, &(datos_hilo_cpu->socket_fd));
  pthread_mutex_unlock(mutex_lista_sockets_cpu);

  // Crear hilo
  if (!crear_hilo_cpu(datos_hilo_cpu, datos->logger))
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
