#include "kernel_scheduler/cpu.h"

#include <commons/collections/list.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/client.h"
#include "utils/io.h"
#include "utils/msg.h"
#include "utils/server.h"

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
  datos->socket = socket_cpu;
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
                pthread_cond_t* cond_fin_cpu,
                t_datos_hilo_escucha* datos_hilo_escucha)
{
  pthread_mutex_lock(mutex_lista_sockets_cpu);
  list_iterate(lista_sockets_cpu, (void*)iterator_shutdown);
  while (!list_is_empty(lista_sockets_cpu))
    pthread_cond_wait(cond_fin_cpu, mutex_lista_sockets_cpu);
  pthread_mutex_unlock(mutex_lista_sockets_cpu);
  list_destroy(lista_sockets_cpu);
  pthread_cond_destroy(cond_fin_cpu);
  pthread_mutex_destroy(mutex_lista_sockets_cpu);
  free(datos_hilo_escucha);
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

bool atender_nueva_cpu(t_datos_hilo_escucha* datos_hilo_escucha, int socket_cpu,
                       t_list* lista_sockets_cpu,
                       pthread_mutex_t* mutex_lista_sockets_cpu,
                       pthread_cond_t* cond_fin_cpu)
{
  // Handshake con CPU
  if (!responder_handshake(socket_cpu, MID_KERNEL_SCHEDULER, logger))
    return false;

  // Obtener ID
  char* id_cpu = obtener_id_cpu(socket_cpu, datos_hilo_escucha->logger);
  if (id_cpu == NULL)
    return false;

  // Inicializar datos hilo cpu
  t_datos_hilo_cpu* datos_hilo_cpu =
      inicializar_datos_hilo_cpu(socket_cpu, lista_sockets_cpu,
                                 mutex_lista_sockets_cpu, cond_fin_cpu, id_cpu);

  // Agregar socket a la lista
  pthread_mutex_lock(mutex_lista_sockets_cpu);
  list_add(lista_sockets_cpu, &(datos_hilo_cpu->socket));
  pthread_mutex_unlock(mutex_lista_sockets_cpu);

  // Crear hilo
  if (!crear_hilo_cpu(datos_hilo_cpu, datos_hilo_escucha->logger))
  {
    pthread_mutex_lock(mutex_lista_sockets_cpu);
    list_remove_element(lista_sockets_cpu, &(datos_hilo_cpu->socket_cpu));
    pthread_mutex_unlock(mutex_lista_sockets_cpu);
    free(datos_hilo_cpu->id);
    free(datos_hilo_cpu);
    return false;
  }

  return true;
}

void cerrar_hilo_cpu(t_datos_hilo_cpu* datos)
{
  close(datos->socket_cpu);
  pthread_mutex_lock(datos->mutex_lista_sockets_cpu);
  list_remove_element(datos->lista_sockets_cpu, &(datos->socket_cpu));
  if (list_is_empty(datos->lista_sockets_cpu))
    pthread_cond_signal(datos->cond_fin_cpu);
  pthread_mutex_unlock(datos->mutex_lista_sockets_cpu);
  free(datos->id);
  free(datos);
}

void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  t_datos_hilo_cpu* datos = (t_datos_hilo_cpu*)datos_hilo_cpu_void;

  while (true)
  {
    int operacion = recibir_operacion(datos->socket_cpu);
    if (operacion == OP_CODE_ERROR)
      break;
    char* buffer = recibir_string(datos->socket_cpu);
    free(buffer);
  }

  // Liberar conexión y eliminar socket de la lista
  cerrar_hilo_cpu(datos);
  return NULL;
}

bool crear_servidor_cpu(pthread_t* thread_server_cpu, int socket_servidor_cpu,
                        t_log* logger)
{
  t_datos_hilo_escucha* datos_hilo_escucha =
      malloc(sizeof(t_datos_hilo_escucha));
  datos_hilo_escucha->socket_espera_cpu = socket_servidor_cpu;
  datos_hilo_escucha->logger = logger;
  if (pthread_create(thread_server_cpu, NULL, hilo_escucha_cpu,
                     datos_hilo_escucha) != 0)
  {
    log_error(logger, "## Error al crear el hilo del servidor de CPU");
    return false;
  }
  return true;
}
