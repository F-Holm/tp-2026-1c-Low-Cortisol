#include "memory_stick/cpu.h"

#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

int create_server_cpu(void)
{
  return iniciar_servidor(NULL);
}

uint16_t get_puerto_cpu(int socket_server_cpu)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  getsockname(socket_server_cpu, (struct sockaddr*)&addr, &len);
  return ntohs(addr.sin_port);
}

void iterator(void* value)
{
  shutdown(*((int*)value), SHUT_RDWR);
}

void* hilo_escucha_cpu(void* datos_hilo_escucha_void)
{
  int socket_espera_cpu =
      ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->socket_fd;
  t_log* logger = ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->logger;

  t_list* lista_sockets = list_create();
  pthread_mutex_t mutex_lista_sockets;
  pthread_mutex_init(&mutex_lista_sockets, NULL);
  pthread_cond_t cond_fin_hilo_escucha;
  pthread_cond_init(&cond_fin_hilo_escucha, NULL);

  while (true)
  {
    int* socket_cpu = malloc(sizeof(int));
    *socket_cpu = esperar_cliente(socket_espera_cpu);
    if (socket_cpu <= 0)
      break;

    log_info(logger, "## Conexión exitosa con CPU");

    // Handshake con CPU
    int id_modulo = recibir_handshake(*socket_cpu);
    if (id_modulo != MID_CPU)
    {
      close(*socket_cpu);
      log_error(logger, "## Error en el Handshake con CPU");
      continue;
    }
    enviar_handshake(MID_MEMORY_STICK, *socket_cpu);
    log_info(logger, "## Handshake exitoso con CPU");

    // Obtener ID
    int codigo_operacion = recibir_operacion(*socket_cpu);
    if (codigo_operacion != OP_ID_CPU)
    {
      close(*socket_cpu);
      log_error(logger, "## Error en la recepción del ID de la CPU");
      continue;
    }
    char* id_cpu = recibir_string(*socket_cpu);
    log_info(logger, "## CPU %s Conectada", id_cpu);
    free(id_cpu);

    // Iniciar y liberar hilo
    t_datos_hilo_cpu* datos_hilo_cpu = malloc(sizeof(t_datos_hilo_cpu));
    datos_hilo_cpu->socket_fd = socket_cpu;
    datos_hilo_cpu->lista_sockets = lista_sockets;
    datos_hilo_cpu->mutex_lista_sockets = &mutex_lista_sockets;
    datos_hilo_cpu->cond_fin_hilo_escucha = &cond_fin_hilo_escucha;
    pthread_mutex_lock(&mutex_lista_sockets);
    list_add(lista_sockets, socket_cpu);
    pthread_mutex_unlock(&mutex_lista_sockets);
    pthread_t hilo_cpu;
    pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos_hilo_cpu);
    pthread_detach(hilo_cpu);
  }

  log_info(logger, "## Cerrando servidor");
  pthread_mutex_lock(&mutex_lista_sockets);
  list_iterate(lista_sockets, (void*)iterator);
  while (!list_is_empty(lista_sockets))
    pthread_cond_wait(&cond_fin_hilo_escucha, &mutex_lista_sockets);
  pthread_mutex_unlock(&mutex_lista_sockets);
  list_destroy(lista_sockets);
  pthread_cond_destroy(&cond_fin_hilo_escucha);
  pthread_mutex_destroy(&mutex_lista_sockets);
  return NULL;
}

void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  int* socket_cpu = ((t_datos_hilo_cpu*)datos_hilo_cpu_void)->socket_fd;
  t_list* lista_sockets =
      ((t_datos_hilo_cpu*)datos_hilo_cpu_void)->lista_sockets;
  pthread_mutex_t* mutex_lista_sockets =
      ((t_datos_hilo_cpu*)datos_hilo_cpu_void)->mutex_lista_sockets;
  pthread_cond_t* cond_fin_hilo_escucha =
      ((t_datos_hilo_cpu*)datos_hilo_cpu_void)->cond_fin_hilo_escucha;
  free(datos_hilo_cpu_void);

  while (true)
  {
    int operacion = recibir_operacion(*socket_cpu);
    if (operacion == OP_CODE_ERROR)
      break;
    char* buffer = recibir_string(*socket_cpu);
    free(buffer);
  }

  // Liberar conexión y eliminar socket de la lista
  liberar_conexion(*socket_cpu);
  pthread_mutex_lock(mutex_lista_sockets);
  list_remove_element(lista_sockets, socket_cpu);
  if (list_is_empty(lista_sockets))
  {
    pthread_cond_signal(cond_fin_hilo_escucha);
  }
  pthread_mutex_unlock(mutex_lista_sockets);
  free(socket_cpu);
  return NULL;
}
