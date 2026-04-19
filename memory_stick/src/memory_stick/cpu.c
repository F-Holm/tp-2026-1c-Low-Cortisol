#include "memory_stick/cpu.h"

#include <bits/pthreadtypes.h>
#include <pthread.h>

#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

int create_server_cpu(t_log* logger)
{
  int ret = iniciar_servidor("0");
  if (ret <= 0)
  {
    log_error(logger, "## Error en la creación del servidor para las CPU");
    return -1;
  }
  log_info(logger, "## Creación del servidor para las CPU exitosa");
  return ret;
}

uint16_t get_puerto_cpu(int socket_server_cpu)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  getsockname(socket_server_cpu, (struct sockaddr*)&addr, &len);
  return ntohs(addr.sin_port);
}

void iterator_shutdown(void* value)
{
  shutdown(*(int*)value, SHUT_RDWR);
}

t_datos_hilo_cpu* inicializar_datos_hilo_cpu(
    int* socket_cpu, t_list* lista_sockets,
    pthread_mutex_t* mutex_lista_sockets, pthread_cond_t* cond_fin_hilo_escucha)
{
  t_datos_hilo_cpu* datos = malloc(sizeof(t_datos_hilo_cpu));
  datos->socket_cpu = socket_cpu;
  datos->lista_sockets = lista_sockets;
  datos->mutex_lista_sockets = mutex_lista_sockets;
  datos->cond_fin_hilo_escucha = cond_fin_hilo_escucha;
  return datos;
}

bool crear_hilo_cpu(t_datos_hilo_cpu* datos_hilo_cpu, t_log* logger)
{
  pthread_t hilo_cpu;
  if (pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos_hilo_cpu) != 0)
  {
    log_error(logger, "## Error en la creación del hilo de la CPU");
    return false;
  }
  pthread_detach(hilo_cpu);
  return true;
}

void cerrar_hilo_escucha(t_list* lista_sockets,
                         pthread_mutex_t* mutex_lista_sockets,
                         pthread_cond_t* cond_fin_hilo_escucha)
{
  pthread_mutex_lock(mutex_lista_sockets);
  list_iterate(lista_sockets, (void*)iterator_shutdown);
  while (!list_is_empty(lista_sockets))
    pthread_cond_wait(cond_fin_hilo_escucha, mutex_lista_sockets);
  pthread_mutex_unlock(mutex_lista_sockets);
  list_destroy(lista_sockets);
  pthread_cond_destroy(cond_fin_hilo_escucha);
  pthread_mutex_destroy(mutex_lista_sockets);
}

bool handshake_cpu(int socket_cpu, t_log* logger)
{
  if (recibir_handshake(socket_cpu) != MID_CPU)
  {
    log_error(logger, "## Error en la recepción del Handshake con CPU");
    return false;
  }
  if (!enviar_handshake(MID_MEMORY_STICK, socket_cpu))
  {
    log_error(logger, "## Error en el envio del Handshake con CPU");
    return false;
  }
  log_info(logger, "## Handshake exitoso con CPU");
  return true;
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

bool atender_nueva_cpu(t_datos_hilo_escucha* params, int* socket_cpu,
                       t_list* lista_sockets,
                       pthread_mutex_t* mutex_lista_sockets,
                       pthread_cond_t* cond_fin_hilo_escucha)
{
  // Handshake con CPU
  if (!handshake_cpu(*socket_cpu, params->logger))
    return false;

  // Obtener ID
  char* id_cpu = obtener_id_cpu(*socket_cpu, params->logger);
  if (id_cpu == NULL)
    return false;
  free(id_cpu);

  // Agregar socket a la lista
  pthread_mutex_lock(mutex_lista_sockets);
  list_add(lista_sockets, socket_cpu);
  pthread_mutex_unlock(mutex_lista_sockets);

  // Inicializar datos hilo cpu
  t_datos_hilo_cpu* datos_hilo_cpu = inicializar_datos_hilo_cpu(
      socket_cpu, lista_sockets, mutex_lista_sockets, cond_fin_hilo_escucha);

  // Crear hilo
  if (!crear_hilo_cpu(datos_hilo_cpu, params->logger))
  {
    pthread_mutex_lock(mutex_lista_sockets);
    list_remove_element(lista_sockets, socket_cpu);
    pthread_mutex_unlock(mutex_lista_sockets);
    free(datos_hilo_cpu);
    return false;
  }
  return true;
}

void* hilo_escucha_cpu(void* datos_hilo_escucha_void)
{
  t_datos_hilo_escucha* params = (t_datos_hilo_escucha*)datos_hilo_escucha_void;

  t_list* lista_sockets = list_create();
  pthread_mutex_t mutex_lista_sockets;
  pthread_cond_t cond_fin_hilo_escucha;

  pthread_mutex_init(&mutex_lista_sockets, NULL);
  pthread_cond_init(&cond_fin_hilo_escucha, NULL);

  while (true)
  {
    int* socket_cpu = malloc(sizeof(int));
    *socket_cpu = esperar_cliente(params->socket_espera_cpu);
    if (*socket_cpu <= 0)
    {
      free(socket_cpu);
      break;
    }

    log_info(params->logger, "## Conexión exitosa con CPU");

    if (!atender_nueva_cpu(params, socket_cpu, lista_sockets,
                           &mutex_lista_sockets, &cond_fin_hilo_escucha))
    {
      close(*socket_cpu);
      free(socket_cpu);
    }
  }

  log_info(params->logger, "## Cerrando servidor");
  cerrar_hilo_escucha(lista_sockets, &mutex_lista_sockets,
                      &cond_fin_hilo_escucha);
  return NULL;
}

void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  t_datos_hilo_cpu* params = (t_datos_hilo_cpu*)datos_hilo_cpu_void;

  while (true)
  {
    int operacion = recibir_operacion(*(params->socket_cpu));
    if (operacion == OP_CODE_ERROR)
      break;
    char* buffer = recibir_string(*(params->socket_cpu));
    free(buffer);
  }

  // Liberar conexión y eliminar socket de la lista
  close(*(params->socket_cpu));
  pthread_mutex_lock(params->mutex_lista_sockets);
  list_remove_element(params->lista_sockets, params->socket_cpu);
  if (list_is_empty(params->lista_sockets))
  {
    pthread_cond_signal(params->cond_fin_hilo_escucha);
  }
  pthread_mutex_unlock(params->mutex_lista_sockets);
  free(params->socket_cpu);
  free(params);
  return NULL;
}
