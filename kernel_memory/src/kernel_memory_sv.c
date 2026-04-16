#include "utils/server.h"
#include "commons/config.h"
#include "commons/collections/list.h"
#include <pthread.h>
#include <commons/log.h>
#include "kernel_memory_sv.h"
#include "utils/msg.h"

#include "kernel_memory_inicializador.h"


t_log* iniciar_logger(t_config* config){return log_create("kernel_memory.log", "kernel_memory", true, log_level_from_string(config_get_string_value(config, "LOG_LEVEL")) );}
t_config* iniciar_config(void){return config_create("kernel_memory.config");}
 
void terminar_comunicacion(int socket_cliente){
  close(socket_cliente);
}

void* escucha_scheduler(void* ptr){
  t_datos_scheduler* datos_scheduler = (t_datos_scheduler*)ptr;
  log_info(datos_scheduler->logger, "## Kernel Scheduler Conectado - FD del socket: %d", datos_scheduler->socket_scheduler);
  char* mensaje ="";
  while (!strcmp(mensaje, "end_communication"))
  {
    mensaje = recibir_mensaje(datos_scheduler->socket_scheduler);
    // QUE HACER CUANDO SE COMUNIC
  }
  terminar_comunicacion(datos_scheduler->socket_scheduler);
}

void* escucha_cpu(void* ptr){
  t_datos_cpu* datos_cpu = (t_datos_cpu*)ptr;
  char* mensaje ="";
  while (!strcmp(mensaje, "end_communication"))
  {
    mensaje = recibir_mensaje(datos_cpu->socket_cpu);
    // QUE HACER CUANDO SE COMUNIC
  }
  terminar_comunicacion(datos_cpu->socket_cpu);
}

void* escucha_swap(void* ptr){
  t_datos_swap* datos_swap = (t_datos_swap*)ptr;
  char* mensaje ="";
  while (!strcmp(mensaje, "end_communication"))
  {
    mensaje = recibir_mensaje(datos_swap->socket_swap);
    // QUE HACER CUANDO SE COMUNIC
  }
  terminar_comunicacion(datos_swap->socket_swap);
}

void* escucha_stick(void* ptr){
  t_datos_stick* datos_stick = (t_datos_stick*)ptr;
  bool conexion_estable = true;
  if (recibir_operacion(datos_stick->socket_stick) == OP_TAMANIO_MEMORIA){
    char* tamanio = recibir_string(datos_stick->socket_stick);
    log_info(datos_stick->logger, "## Memory Stick de %d bytes Conectada", tamanio ); 
    datos_stick->tamanio_stick = stoi(tamanio); 
    free(tamanio);
    // crear Lista de conexion de sticks 
    while (conexion_estable)
    {
      switch (recibir_operacion(datos_stick->socket_stick))
      {
      case OP_PAQUETE:
        t_list* paquete = recibir_paquete(datos_stick->socket_stick);
         log_info(datos_stick->logger, "Llego un paquete de la memory stick" ); 
         //comunicaciones 
        break;
      case OP_CODE_ERROR:
        conexion_estable= false ; 
      break;

        default:
        break;
      }
    }
  }else(){
  log_info(datos_stick->loger, "No se pudo realizar la conexion con la stick ya que no se envio la operacion de tamaño");
  terminar_comunicacion(datos_stick->socket_stick);
  }
    terminar_comunicacion(datos_stick->socket_stick);
}

void empezar_escucha_scheduler(t_datos_scheduler* datos_scheduler){
    pthread_t hilo_escucha;
    pthread_create(&hilo_escucha, NULL, escucha_scheduler, &datos_scheduler);
    pthread_detach(hilo_escucha);
}

void empezar_escucha_cpu(t_datos_cpu* datos_cpu){
    pthread_t hilo_escucha;
    pthread_create(&hilo_escucha, NULL, escucha_cpu, &datos_cpu);
    pthread_detach(hilo_escucha);
}

void empezar_escucha_stick(t_datos_stick* datos_stick){
    pthread_t hilo_escucha;
    pthread_create(&hilo_escucha, NULL, escucha_stick, &datos_stick);
    pthread_detach(hilo_escucha);
}

void empezar_escucha_swap(t_datos_swap* datos_swap){
    pthread_t hilo_escucha;
    pthread_create(&hilo_escucha, NULL, escucha_swap, &datos_swap);
    pthread_detach(hilo_escucha);
}


void handshake(t_datos_kernel_mem* datos_kernel_memory, int client_socket){
  log_info(datos_kernel_memory->logger, "Servidor a la espera de handshake");
   int identificador = recibir_handshake(client_socket);
    switch (identificador)
    {
    case MID_KERNEL_SCHEDULER:
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      log_info(datos_kernel_memory->logger, "Se ha conectado el kernel scheduler!");
      t_datos_scheduler* datos_scheduler = inicializar_datos_scheduler(client_socket,datos_kernel_memory->logger);
      empezar_escucha_scheduler(datos_scheduler);
      break;

    case MID_CPU:
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      log_info(datos_kernel_memory->logger, "Se ha conectado una CPU!");
      t_datos_cpu* datos_cpu= inicializar_datos_cpu(client_socket,datos_kernel_memory->logger);
      empezar_escucha_cpu(datos_cpu);
      break;

    case MID_MEMORY_STICK:
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      log_info(datos_kernel_memory->logger, "Se ha conectado una memory Stick!");
      t_datos_stick* datos_stick= inicializar_datos_stick(client_socket,datos_kernel_memory->logger);
      empezar_escucha_stick(datos_stick);
      break;

    case MID_SWAP:
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      log_info(datos_kernel_memory->logger, "Se ha conectado el SWAP!");
      t_datos_swap* datos_swap= inicializar_datos_swap(client_socket,datos_kernel_memory->logger);
      empezar_escucha_swap(datos_swap);
      break;

    default:
      break;
    }   
}


void* accept_cliente(void* ptr) {
    t_datos_kernel_mem* datos_kernel_memory = (t_datos_kernel_mem*)ptr;
    log_info(datos_kernel_memory->logger, "Servidor a la espera de un cliente");
    int socket_cliente = esperar_cliente(datos_kernel_memory->socket_kernel_memory);
    log_info(datos_kernel_memory->logger, "Se ha aceptado a un cliente!");
    handshake(datos_kernel_memory, socket_cliente);
}


void hilo_aceptacion(t_datos_kernel_mem* server_data){
  pthread_t hilo_acceptacion;
  pthread_create(&hilo_acceptacion, NULL, accept_cliente, &server_data);
  pthread_join(hilo_acceptacion, NULL);
}



int main(int argc, char* argv[])
{ 
  t_config* config = iniciar_config();
  t_log* logger = iniciar_logger(config);
  int socket_kernel_memory = iniciar_servidor(config_get_string_value(config, "PUERTO_KERNEL_MEMORY"));

  t_datos_kernel_mem* datos_kernel = inicializar_datos_kernel_memory(socket_kernel_memory, logger);

  while (true)
  {
    hilo_aceptacion(datos_kernel);
  }

  free(datos_kernel);

  return 0;
}

