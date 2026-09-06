#ifndef MEMORY_STICK_CPU_H_
#define MEMORY_STICK_CPU_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "memory_stick/memory_stick.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/logger.h"

typedef struct
{
  int socket_espera_cpu;
  t_logger* logger;
  t_ms_recursos* ms_recursos;
} t_datos_hilo_escucha;

typedef struct
{
  int socket_cpu;
  t_list* lista_sockets;
  pthread_mutex_t* mutex_lista_sockets;
  pthread_cond_t* cond_fin_hilo_escucha;
  t_ms_recursos* ms_recursos;
} t_datos_hilo_cpu;

int create_server_cpu(t_logger* logger);
uint16_t get_puerto_cpu(int socket_server_cpu);
void iterator_shutdown(void* value);
t_datos_hilo_cpu* inicializar_datos_hilo_cpu(
    int socket_cpu, t_list* lista_sockets, pthread_mutex_t* mutex_lista_sockets,
    pthread_cond_t* cond_fin_hilo_escucha, t_ms_recursos* ms_recursos);
bool crear_hilo_cpu(t_datos_hilo_cpu* datos_hilo_cpu, t_logger* logger);
void cerrar_hilo_escucha(t_list* lista_sockets,
                         pthread_mutex_t* mutex_lista_sockets,
                         pthread_cond_t* cond_fin_hilo_escucha,
                         t_datos_hilo_escucha* pardatos_hilo_escuchaams);
bool handshake_cpu(int socket_cpu, t_logger* logger);
char* obtener_id_cpu(int socket_cpu, t_logger* logger);
bool atender_nueva_cpu(t_datos_hilo_escucha* datos_hilo_escucha, int socket_cpu,
                       t_list* lista_sockets,
                       pthread_mutex_t* mutex_lista_sockets,
                       pthread_cond_t* cond_fin_hilo_escucha,
                       t_ms_recursos* t_ms_recursos);
void* hilo_escucha_cpu(void* datos_hilo_escucha_void);
void* manejar_cliente_cpu(void* datos_hilo_cpu_void);
void cerrar_hilo_cpu(t_datos_hilo_cpu* datos_hilo_cpu);
bool crear_servidor_cpu(pthread_t* thread_server_cpu, int socket_servidor_cpu,
                        t_logger* logger, t_ms_recursos* ms_recursos);

#endif /* MEMORY_STICK_CPU_H_ */
