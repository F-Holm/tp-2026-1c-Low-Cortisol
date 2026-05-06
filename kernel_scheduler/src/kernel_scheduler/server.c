#include "kernel_scheduler/server.h"

#include <commons/collections/list.h>

#include "kernel_scheduler/cpu.h"
#include "kernel_scheduler/io.h"

void cerrar_hilo_escucha(int sockets_io, t_list* lista_sockets,
                         pthread_mutex_t* mutex_lista_sockets,
                         pthread_cond_t* cond_fin_hilo_escucha,
                         t_datos_hilo_escucha* datos_hilo_escucha)
{
  cerrar_io(sockets_io);
  cerrar_cpu(mutex_lista_sockets, cond_fin_hilo_escucha);
  free(datos_hilo_escucha);
}

int crear_socket_servidor(char* puerto, t_log* logger)
{
  int ret = iniciar_servidor(puerto);
  if (ret <= 0)
  {
    log_error(logger, "## Error en la creación del servidor");
    return -1;
  }
  log_info(logger, "## Creación del servidor exitosa");
  return ret;
}

t_datos_hilo_escucha* inicializar_datos_hilo_escucha(int socket_server,
                                                     t_log* logger)
{
  t_datos_hilo_escucha* datos = malloc(sizeof(t_datos_hilo_escucha));
  datos->socket_server = socket_server;
  datos->logger = logger;
  return datos;
}

void* hilo_escucha(void* datos_hilo_escucha_void)
{
  t_datos_hilo_escucha* datos_hilo_escucha =
      (t_datos_hilo_escucha*)datos_hilo_escucha_void;

  int sockets_io[3] = {-1, -1, -1};
  t_list* lista_sockets_cpu = list_create();
  pthread_mutex_t mutex_lista_sockets_cpu;
  pthread_cond_t cond_fin_hilo_escucha;

  pthread_mutex_init(&mutex_lista_sockets_cpu, NULL);
  pthread_cond_init(&cond_fin_hilo_escucha, NULL);

  while (true)
  {
    bool manejo_exitoso = true;
    int socket_fd = esperar_cliente(datos_hilo_escucha->socket_espera_cpu);
    if (socket_fd <= 0)
      break;

    switch (recibir_handshake(socket_fd))
    {
      case MID_CPU:
        manejo_exitoso =
            atender_nueva_cpu(datos_hilo_escucha, socket_fd, lista_sockets_cpu,
                              &mutex_lista_sockets_cpu, &cond_fin_hilo_escucha);
        break;
      case MID_IO:
        manejo_exitoso =
            atender_nuevo_io(datos_hilo_escucha, sockets_io, socket_fd);
        break;
      default:
        log_info(datos_hilo_escucha->logger,
                 "## Recepción de handshake no válido");
        manejo_exitoso = false;
        break;
    }
    if (!manejo_exitoso)
      close(socket_fd);
  }

  log_info(datos_hilo_escucha->logger, "## Cerrando servidor");
  cerrar_hilo_escucha(sockets_io, lista_sockets_cpu, &mutex_lista_sockets_cpu,
                      &cond_fin_hilo_escucha, datos_hilo_escucha);
  return NULL;
}
