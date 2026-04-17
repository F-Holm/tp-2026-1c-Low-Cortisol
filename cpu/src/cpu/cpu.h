#ifndef MEMORY_STICK_MEMORY_STICK_H_
#define MEMORY_STICK_MEMORY_STICK_H_

#include <commons/config.h>
#include "utils/msg.h"
#include <stdio.h>
#include <pthread.h>

typedef struct {
    char* ip;
    int puerto;
    int socket;
} t_memory_stick;

typedef struct
{
    t_memory_stick memory_stick;
    struct t_nodo_lista_memory_stick *sgte;
} t_nodo_lista_memory_stick;

typedef struct {
    pthread_t kernel_memory_hilo;
    pthread_mutex_t mutex_memory_sticks;
} t_hilo_cpu;

typedef struct {
    char* id;

    int socket_kernel_memory;
    int socket_kernel_scheduler;

    t_nodo_lista_memory_stick* memory_sticks;

    t_hilo_cpu hilos;

    t_log* logger;
    t_config* config;
} t_cpu;

t_memory_stick crear_nodo(char* ip, int puerto, int socket);
void iniciar_hilo(void* arg);
void* escuchar_kernel_memory(void* arg);

#endif /* MEMORY_STICK_MEMORY_STICK_H_ */
