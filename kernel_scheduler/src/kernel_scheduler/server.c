#include "kernel_scheduler/server.h"

#include <commons/collections/list.h>
#include <pthread.h>

#include "kernel_scheduler/cpu.h"
#include "kernel_scheduler/io.h"
#include "kernel_scheduler/misc.h"
#include "utils/msg.h"
#include "utils/server.h"

static void cerrar_hilo_escucha(t_io* estructuras_io, t_list* lista_sockets_cpu,
                                pthread_mutex_t* mutex_lista_sockets_cpu,
                                pthread_cond_t* cond_fin_cpu,
                                t_datos_servidor_escucha* datos);

int crear_socket_servidor(char* puerto, t_logger* logger)
{
  int ret = iniciar_servidor(puerto);
  if (ret <= 0)
  {
    logger_error(logger, " Error en la creación del servidor");
    return -1;
  }
  logger_info(logger, " Creación del servidor exitosa");
  return ret;
}

void inicializar_datos_server_escucha(
    t_datos_servidor_escucha* datos, int socket_server, t_logger* logger,
    t_lista_mutex* lista_mutex, t_colas* colas,
    t_socket_kernel_memory* socket_kernel_memory, char* path_proceso_inicial)
{
  datos->socket_server = socket_server;
  datos->logger = logger;
  datos->lista_mutex = lista_mutex;
  datos->colas = colas;
  datos->socket_km = socket_kernel_memory;
  datos->path_proceso_inicial = path_proceso_inicial;
}

void servidor_escucha(t_datos_servidor_escucha* datos)
{
  t_io* estructuras_io = crear_estructuras_io();
  t_list* lista_sockets_cpu = list_create();
  pthread_mutex_t mutex_lista_sockets_cpu;
  pthread_cond_t cond_fin_cpu;

  pthread_mutex_init(&mutex_lista_sockets_cpu, NULL);
  pthread_cond_init(&cond_fin_cpu, NULL);

  cambio_new_ready(datos->colas, datos->path_proceso_inicial, 0);
  while (true)
  {
    bool manejo_exitoso = true;
    int socket_fd = esperar_cliente(datos->socket_server);
    if (socket_fd <= 0)
    {
      break;
    }

    switch (recibir_handshake(socket_fd))
    {
      case MID_CPU:
        manejo_exitoso = atender_nueva_cpu(
            socket_fd, lista_sockets_cpu, &mutex_lista_sockets_cpu,
            &cond_fin_cpu, datos->logger, datos->lista_mutex, datos->colas,
            estructuras_io, datos->socket_km, datos->socket_server);
        break;
      case MID_IO:
        manejo_exitoso =
            atender_nuevo_io(estructuras_io, socket_fd, datos->colas,
                             datos->colas->ready.cola_multi_nivel);
        break;
      default:
        logger_info(datos->logger, " Recepción de handshake no válido");
        manejo_exitoso = false;
        break;
    }
    if (!manejo_exitoso)
    {
      close(socket_fd);
    }
  }

  cerrar_hilo_escucha(estructuras_io, lista_sockets_cpu,
                      &mutex_lista_sockets_cpu, &cond_fin_cpu, datos);
}

static void cerrar_hilo_escucha(t_io* estructuras_io, t_list* lista_sockets_cpu,
                                pthread_mutex_t* mutex_lista_sockets_cpu,
                                pthread_cond_t* cond_fin_cpu,
                                t_datos_servidor_escucha* datos)
{
  logger_info(datos->logger, " Cerrando servidor");
  cerrar_cpu(lista_sockets_cpu, mutex_lista_sockets_cpu, cond_fin_cpu,
             datos->colas);
  cerrar_io(estructuras_io);
  logger_info(datos->logger, " Servidor cerrado");
}
