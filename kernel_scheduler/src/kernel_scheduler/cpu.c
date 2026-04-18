#include "cpu.h"

#include <commons/collections/list.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/kernel_scheduler.h"
#include "utils/client.h"
#include "utils/io.h"
#include "utils/msg.h"
#include "utils/server.h"

void iterator(void* value)
{
  shutdown(*((int*)value), SHUT_RDWR);
}

void* hilo_escucha_server(void* datos_hilo_escucha_void)
{
  int socket_server_cpu_io =
      ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->socket_fd;
  t_log* logger = ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->logger;

  int sockets_io[3];

  t_list* lista_sockets_cpu = list_create();
  pthread_mutex_t mutex_lista_sockets;
  pthread_mutex_init(&mutex_lista_sockets, NULL);
  pthread_cond_t cond_fin_hilo_escucha;
  pthread_cond_init(&cond_fin_hilo_escucha, NULL);

  while (true)
  {
    int* socket_cpu_io = malloc(sizeof(int));
    *socket_cpu_io = esperar_cliente(socket_server_cpu_io);
    if (*socket_cpu_io <= 0){
      free(socket_cpu_io);
      break;
    }
    // que onda con este log?
    log_info(logger, "## Conectado con cpu");

    // Handshake con CPU o IO
    int id_modulo = recibir_handshake(*socket_cpu_io);
    if (id_modulo == MID_IO)
    {
      enviar_handshake(MID_KERNEL_SCHEDULER, *socket_cpu_io);
      log_info(logger, "## Handshake exitoso con IO");

      // obtener tipo de io
      int operacion = recibir_operacion(*socket_cpu_io);
      if (operacion != OP_TIPO_IO)
      {
        log_warning(logger, "## Código de operación no válido: %d", operacion);
        close(*socket_cpu_io);
        free(socket_cpu_io);
        continue;
      }

      char* tipo_io = recibir_string(*socket_cpu_io);
      if (strcmp(V_TIPO_IO[E_STDIN], tipo_io) == 0)
        sockets_io[E_STDIN] = *socket_cpu_io;
      else if (strcmp(V_TIPO_IO[E_STDOUT], tipo_io) == 0)
        sockets_io[E_STDOUT] = *socket_cpu_io;
      else if (strcmp(V_TIPO_IO[E_SLEEP], tipo_io) == 0)
        sockets_io[E_SLEEP] = *socket_cpu_io;
      else
      {
        log_warning(logger, "## IO de tipo %s Conectada", tipo_io);
        free(tipo_io);
        close(*socket_cpu_io);
        free(socket_cpu_io);
        continue;
      }
      log_info(logger, "## IO de tipo %s Conectada", tipo_io);
      free(tipo_io);
      free(socket_cpu_io);
      continue;
    }
    else if (id_modulo != MID_CPU)
    {
      close(*socket_cpu_io);
      log_error(logger, "## Error en el Handshake con CPU");
      free(socket_cpu_io);
      continue;
    }
    enviar_handshake(MID_KERNEL_SCHEDULER, *socket_cpu_io);
    log_info(logger, "## Handshake exitoso con CPU");

    // Obtener ID de cpu
    int codigo_operacion = recibir_operacion(*socket_cpu_io);
    if (codigo_operacion != OP_ID_CPU)
    {
      close(*socket_cpu_io);
      log_error(logger, "## Error en la recepción del ID de la CPU");
      free(socket_cpu_io);
      continue;
    }
    char* id_cpu = recibir_string(*socket_cpu_io);
    log_info(logger, "## CPU %s Conectada", id_cpu);
    free(id_cpu);

    // Iniciar hilo
    t_datos_hilo_cpu* datos_hilo_cpu = malloc(sizeof(t_datos_hilo_cpu));
    datos_hilo_cpu->socket_fd = socket_cpu_io;
    datos_hilo_cpu->lista_sockets = lista_sockets_cpu;
    datos_hilo_cpu->mutex_lista_sockets = &mutex_lista_sockets;
    datos_hilo_cpu->cond_fin_hilo_escucha = &cond_fin_hilo_escucha;
    pthread_mutex_lock(&mutex_lista_sockets);
    list_add(lista_sockets_cpu, socket_cpu_io);
    pthread_mutex_unlock(&mutex_lista_sockets);
    pthread_t hilo_cpu;
    pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos_hilo_cpu);
    pthread_detach(hilo_cpu);
  }

  // Liberar hilo
  log_info(logger, "## Cerrando servidor");
  pthread_mutex_lock(&mutex_lista_sockets);
  list_iterate(lista_sockets_cpu, (void*)iterator);
  while (!list_is_empty(lista_sockets_cpu))
    pthread_cond_wait(&cond_fin_hilo_escucha, &mutex_lista_sockets);
  pthread_mutex_unlock(&mutex_lista_sockets);
  list_destroy(lista_sockets_cpu);
  pthread_cond_destroy(&cond_fin_hilo_escucha);
  pthread_mutex_destroy(&mutex_lista_sockets);

  // Cerrar sockets IO
  for (int i = 0; i < 3; i++)
    close(sockets_io[i]);
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
