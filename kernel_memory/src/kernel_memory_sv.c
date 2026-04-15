#include "utils/server.h"
#include "commons/config.h"
#include "commons/collections/list.h"
#include <pthread.h>

t_log* logger;
//HAY QUE LIBERAR ESTO TAMBIEN :::>>>
t_list* MEMORY_STICK_CONNECTED = list_create();
t_list* CPU_CONNECTED = list_create();

typedef struct 
{
  int socket_memory_stick;

}t_memory_stick_connection;

typedef struct 
{
  int socket_cpu;
}t_cpu_connection;

typedef struct 
{
  int socket_swap;
}t_swap_connection;

typedef struct  
{
  int kernel_scheduler;
}t_kernel_scheduler_connection;

t_config* iniciar_config(void)
{return config_create("kernel_memory.config");}

int iniciar_servidor(char* puerto){

    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, puerto, &hints, &servinfo);
    // Creamos el socket de escucha del servidor
    int socket_servidor = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);
    // Asociamos el socket a un puerto
    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    // Escuchamos las conexiones entrantes
    listen(socket_servidor, SOMAXCONN);
    
    freeaddrinfo(servinfo);

    log_trace(logger, "Listo para la escucha");
    
    return socket_servidor;
}

void* accept_cliente(void* ptr) {
    int server_socket = *(int*)ptr;
    int socket_cliente = esperar_cliente(server_socket);
    
}

int esperar_cliente(int socket_servidor)
{
  // Aceptamos un nuevo cliente
  // Si se cerró el servidor, socket_cliente == -1
  int socket_cliente = accept(socket_servidor, NULL, NULL);
  log_info(logger, (socket_cliente == -1 ? "falló accept(socket)"
                                         : "Se conecto un cliente!"));
  if (handshake(socket_cliente)){
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, hilo_escuchando, &socket_cliente);
  pthread_detach(hilo_escucha);
  }else{
    log_info(logger,"Handshake Fallido");
  }
  return socket_cliente;
}

bool handshake(int socket_cliente)
{ 
  char* ack = recibir_mensaje(socket_cliente);
  if (socket_cliente > -1 && ack != NULL) {
    switch (ack)
    {
    case strcmp(ack, "memory_stick"):
      t_memory_stick_connection memory_stick_informacion = {};
      MEMORY_STICK_CONNECTED = list_add(MEMORY_STICK_CONNECTED, memory_stick_informacion);

      break;
    case strcmp(ack, "cpu"):
      t_cpu_connection cpu_informacion = {};
      CPU_CONNECTED = list_add(CPU_CONNECTED, cpu_informacion);

      break;

    default:
      break;
    }
    enviar_mensaje("kernel_memory", socket_cliente);
    free(ack);
    return true;
  }
  return false;
}

void hilo_acceptacion(int server_socket){
  pthread_t hilo_acceptacion;
  pthread_create(&hilo_acceptacion, NULL, accept_cliente, &server_socket);
  pthread_join(hilo_acceptacion);
}

void hilo_escuchando(int socket_cliente){
  char* mensaje ='';
  while (!strcmp(mensaje, 'end_communication'))
  {
    mensaje = recibir_mensaje(socket_cliente);
    // QUE HACER CUANDO SE COMUNIC
  }
  terminar_comunicacion(socket_cliente);
}

void terminar_comunicacion(int socket_cliente){
  free(socket_cliente);
}

int recibir_operacion(int socket_cliente)
{
  int cod_op;
  if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
    return cod_op;
  else
  {
    close(socket_cliente);
    return -1;
  }
}

void* recibir_buffer(int* size, int socket_cliente)
{
  void* buffer;

  recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
  buffer = malloc(*size);
  recv(socket_cliente, buffer, *size, MSG_WAITALL);

  return buffer;
}

char* recibir_mensaje(int socket_cliente)
{
  int size;
  return recibir_buffer(&size, socket_cliente);
  // acordarse de liberar memoria dinamica del puntero retornado
}

/*
t_list* recibir_paquete(int socket_cliente)
{
  int size;
  int desplazamiento = 0;
  void* buffer;
  t_list* valores = list_create();
  int tamanio;

  buffer = recibir_buffer(&size, socket_cliente);
  while (desplazamiento < size)
  {
    memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    char* valor = malloc(tamanio + 1);
    memcpy(valor, buffer + desplazamiento, tamanio);
    valor[tamanio] = '\0';
    desplazamiento += tamanio;
    list_add(valores, valor);
  }
  free(buffer);
  return valores;
}
*/
t_log* iniciar_logger(t_config* config){

  log_create('kernel_memory.log', 'kernel_memory', true, LOG_LEVEL_INFO);
}

int main(int argc, char* argv[])
{ 
  t_config* config-> iniciar_config();
  iniciar_logger()
  char* puerto = config_get_int_value(config, 'PUERTO_KERNEL_MEMORY');
  int socket_kernel_memory = iniciar_servidor(puerto);

  while (true)
  {
      hilo_acceptacion(socket_kernel_memory);
  }
  

  return 0;
}