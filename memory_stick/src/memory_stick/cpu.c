#include "memory_stick/cpu.h"

#include <bits/pthreadtypes.h>
#include <pthread.h>

#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

int create_server_cpu(t_logger* logger)
{
  int ret = iniciar_servidor("0");
  if (ret <= 0)
  {
    logger_error(logger, "## Error en la creación del servidor para las CPU");
    return -1;
  }
  logger_info(logger, "## Creación del servidor para las CPU exitosa");
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
    int socket_cpu, t_list* lista_sockets, pthread_mutex_t* mutex_lista_sockets,
    pthread_cond_t* cond_fin_hilo_escucha)
{
  t_datos_hilo_cpu* datos = malloc(sizeof(t_datos_hilo_cpu));
  datos->socket_cpu = socket_cpu;
  datos->lista_sockets = lista_sockets;
  datos->mutex_lista_sockets = mutex_lista_sockets;
  datos->cond_fin_hilo_escucha = cond_fin_hilo_escucha;
  return datos;
}

bool crear_hilo_cpu(t_datos_hilo_cpu* datos_hilo_cpu, t_logger* logger)
{
  pthread_t hilo_cpu;
  if (pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos_hilo_cpu) != 0)
  {
    logger_error(logger, "## Error en la creación del hilo de la CPU");
    return false;
  }
  pthread_detach(hilo_cpu);
  return true;
}

void cerrar_hilo_escucha(t_list* lista_sockets,
                         pthread_mutex_t* mutex_lista_sockets,
                         pthread_cond_t* cond_fin_hilo_escucha,
                         t_datos_hilo_escucha* datos_hilo_escucha)
{
  pthread_mutex_lock(mutex_lista_sockets);
  list_iterate(lista_sockets, (void*)iterator_shutdown);
  while (!list_is_empty(lista_sockets))
    pthread_cond_wait(cond_fin_hilo_escucha, mutex_lista_sockets);
  pthread_mutex_unlock(mutex_lista_sockets);
  list_destroy(lista_sockets);
  pthread_cond_destroy(cond_fin_hilo_escucha);
  pthread_mutex_destroy(mutex_lista_sockets);
  free(datos_hilo_escucha);
}

bool handshake_cpu(int socket_cpu, t_logger* logger)
{
  if (recibir_handshake(socket_cpu) != MID_CPU)
  {
    logger_error(logger, "## Error en la recepción del Handshake con CPU");
    return false;
  }
  if (!enviar_handshake(MID_MEMORY_STICK, socket_cpu))
  {
    logger_error(logger, "## Error en el envio del Handshake con CPU");
    return false;
  }
  logger_info(logger, "## Handshake exitoso con CPU");
  return true;
}

char* obtener_id_cpu(int socket_cpu, t_logger* logger)
{
  if (recibir_operacion(socket_cpu) != OP_ID_CPU)
  {
    logger_error(logger, "## Error en la recepción del ID de la CPU");
    return NULL;
  }
  char* id_cpu = recibir_string(socket_cpu);
  logger_info(logger, "## CPU %s Conectada", id_cpu);
  return id_cpu;
}

bool atender_nueva_cpu(t_datos_hilo_escucha* datos_hilo_escucha, int socket_cpu,
                       t_list* lista_sockets,
                       pthread_mutex_t* mutex_lista_sockets,
                       pthread_cond_t* cond_fin_hilo_escucha)
{
  // Handshake con CPU
  if (!handshake_cpu(socket_cpu, datos_hilo_escucha->logger))
    return false;

  // Obtener ID
  char* id_cpu = obtener_id_cpu(socket_cpu, datos_hilo_escucha->logger);
  if (id_cpu == NULL)
    return false;
  free(id_cpu);

  // Inicializar datos hilo cpu
  t_datos_hilo_cpu* datos_hilo_cpu = inicializar_datos_hilo_cpu(
      socket_cpu, lista_sockets, mutex_lista_sockets, cond_fin_hilo_escucha);

  // Agregar socket a la lista
  pthread_mutex_lock(mutex_lista_sockets);
  list_add(lista_sockets, &(datos_hilo_cpu->socket_cpu));
  pthread_mutex_unlock(mutex_lista_sockets);

  // Crear hilo
  if (!crear_hilo_cpu(datos_hilo_cpu, datos_hilo_escucha->logger))
  {
    pthread_mutex_lock(mutex_lista_sockets);
    list_remove_element(lista_sockets, &(datos_hilo_cpu->socket_cpu));
    pthread_mutex_unlock(mutex_lista_sockets);
    free(datos_hilo_cpu);
    return false;
  }
  return true;
}

void* hilo_escucha_cpu(void* datos_hilo_escucha_void)
{
  t_datos_hilo_escucha* datos_hilo_escucha =
      (t_datos_hilo_escucha*)datos_hilo_escucha_void;

  t_list* lista_sockets = list_create();
  pthread_mutex_t mutex_lista_sockets;
  pthread_cond_t cond_fin_hilo_escucha;

  pthread_mutex_init(&mutex_lista_sockets, NULL);
  pthread_cond_init(&cond_fin_hilo_escucha, NULL);

  while (true)
  {
    int socket_cpu = esperar_cliente(datos_hilo_escucha->socket_espera_cpu);
    if (socket_cpu <= 0)
      break;

    logger_info(datos_hilo_escucha->logger, "## Conexión exitosa con CPU");

    if (!atender_nueva_cpu(datos_hilo_escucha, socket_cpu, lista_sockets,
                           &mutex_lista_sockets, &cond_fin_hilo_escucha))
      close(socket_cpu);
  }

  logger_info(datos_hilo_escucha->logger, "## Cerrando servidor");
  cerrar_hilo_escucha(lista_sockets, &mutex_lista_sockets,
                      &cond_fin_hilo_escucha, datos_hilo_escucha);
  return NULL;
}

void cerrar_hilo_cpu(t_datos_hilo_cpu* datos_hilo_cpu)
{
  close(datos_hilo_cpu->socket_cpu);
  pthread_mutex_lock(datos_hilo_cpu->mutex_lista_sockets);
  list_remove_element(datos_hilo_cpu->lista_sockets,
                      &(datos_hilo_cpu->socket_cpu));
  if (list_is_empty(datos_hilo_cpu->lista_sockets))
    pthread_cond_signal(datos_hilo_cpu->cond_fin_hilo_escucha);
  pthread_mutex_unlock(datos_hilo_cpu->mutex_lista_sockets);
  free(datos_hilo_cpu);
}

void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  t_datos_hilo_cpu* datos_hilo_cpu = (t_datos_hilo_cpu*)datos_hilo_cpu_void;

  while (true)
  {
    int operacion = recibir_operacion(datos_hilo_cpu->socket_cpu);
    if (operacion == OP_CODE_ERROR)
      break;
    char* buffer = recibir_string(datos_hilo_cpu->socket_cpu);
    free(buffer);
  }

  // Liberar conexión y eliminar socket de la lista
  cerrar_hilo_cpu(datos_hilo_cpu);
  return NULL;
}

bool crear_servidor_cpu(pthread_t* thread_server_cpu, int socket_servidor_cpu,
                        t_logger* logger)
{
  t_datos_hilo_escucha* datos_hilo_escucha =
      malloc(sizeof(t_datos_hilo_escucha));
  datos_hilo_escucha->socket_espera_cpu = socket_servidor_cpu;
  datos_hilo_escucha->logger = logger;
  if (pthread_create(thread_server_cpu, NULL, hilo_escucha_cpu,
                     datos_hilo_escucha) != 0)
  {
    logger_error(logger, "## Error al crear el hilo del servidor de CPU");
    return false;
  }
  return true;
}
