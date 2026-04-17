#include "cpu.h"
#include <commons/collections/list.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

void* hilo_escucha_server(void* datos_hilo_escucha_void)
{
  int socket_server_cpu_io = ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->socket_fd;
  t_log* logger = ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->logger;
  
  t_list* lista_sockets_cpu = list_create();
  t_list* lista_sockets_io = list_create();
  pthread_mutex_t mutex_lista_sockets;
  pthread_mutex_init(&mutex_lista_sockets, NULL);
  pthread_cond_t cond_fin_hilo_escucha;
  pthread_cond_init(&cond_fin_hilo_escucha, NULL);

  while (true)
  {
    int* socket_cpu_io = malloc(sizeof(int));
    *socket_cpu_io = esperar_cliente(socket_server_cpu_io);
    if (socket_cpu_io <= 0)
      break;

      //que onda con este log?
    log_info(logger, "## Conectado con cpu");

    // Handshake con CPU o IO
    int id_modulo = recibir_handshake(*socket_cpu_io);
    if(id_modulo == MID_IO) 
    {
      enviar_handshake(MID_KERNEL_SCHEDULER, *socket_cpu_io);
      log_info(logger, "## Handshake exitoso con IO");
      list_add(lista_sockets_io,socket_cpu_io);
      continue;
    }
    else if (id_modulo != MID_CPU)
    {
      close(*socket_cpu_io);
      log_error(logger, "## Error en el Handshake con CPU");
      continue;
    }
    else {
      enviar_handshake(MID_KERNEL_SCHEDULER, *socket_cpu_io);
      log_info(logger, "## Handshake exitoso con CPU");
    }

    //obtener ID de cpu o tipo de io
    int codigo_operacion = recibir_operacion(*socket_cpu_io);
    if(codigo_operacion==OP_ID_CPU)
    {
      char* id_cpu = recibir_string(*socket_cpu_io);
      log_info(logger, "## CPU %s Conectada", id_cpu);
      free(id_cpu);
    }
    else if (codigo_operacion==OP_TIPO_IO)
    {
      char* tipo_io = recibir_string(*socket_cpu_io);
      log_info(logger, "## IO de tipo %s Conectada", tipo_io);
      free(tipo_io);
    }
    else 
    {
      close(*socket_cpu_io);
      log_error(logger, "## Error en la recepción del ID");
      continue;
    }

    //Iniciar y liberar hilo
    
   


  //cerrar y liberar recursos
}
}