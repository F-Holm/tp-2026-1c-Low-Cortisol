#ifndef MEMORY_STICK_CPU_H_
#define MEMORY_STICK_CPU_H_

#include <arpa/inet.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct
{
  int socket_espera_cpu;
  t_log* logger;
} t_datos_hilo_escucha;

typedef struct
{
  int* socket_cpu;
  t_list* lista_sockets;
  pthread_mutex_t* mutex_lista_sockets;
  pthread_cond_t* cond_fin_hilo_escucha;
} t_datos_hilo_cpu;

int create_server_cpu(t_log* logger);
uint16_t get_puerto_cpu(int socket_server_cpu);
void iterator_shutdown(void* value);
t_datos_hilo_cpu* inicializar_datos_hilo_cpu(
    int* socket_cpu, t_list* lista_sockets,
    pthread_mutex_t* mutex_lista_sockets,
    pthread_cond_t* cond_fin_hilo_escucha);
bool crear_hilo_cpu(t_datos_hilo_cpu* datos_hilo_cpu);
void cerrar_hilo_escucha(t_list* lista_sockets,
                         pthread_mutex_t* mutex_lista_sockets,
                         pthread_cond_t* cond_fin_hilo_escucha);
void* hilo_escucha_cpu(void* datos_hilo_escucha_void);
void* manejar_cliente_cpu(void* datos_hilo_cpu_void);

#endif /* MEMORY_STICK_CPU_H_ */
