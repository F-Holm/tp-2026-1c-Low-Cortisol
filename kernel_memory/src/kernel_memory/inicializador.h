#include <commons/collections/list.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "commons/collections/list.h"
#include "kernel_memory/estructuras.h"
#include "utils/logger.h"

t_datos_kernel_mem* inicializar_datos_kernel_memory(
    int socket_kernel_memory, char* scripts_basepath, int instruction_delay,
    int compaction_delay, int segment_max_size,
    t_allocation_strategy allocation_strategy, t_logger* logger);
t_datos_scheduler* inicializar_datos_scheduler(
    int socket_scheduler, t_list* procesos, char* scripts_basepath,
    pthread_mutex_t* mutex_procesos, t_memoria_principal* memoria_principal,
    t_logger* logger);
t_datos_cpu* inicializar_datos_cpu(int socket_cpu, t_list* procesos,
                                   pthread_mutex_t* mutex_procesos,
                                   int instruction_delay, t_logger* logger);
t_datos_stick* inicializar_datos_stick(int socket_stick, t_logger* logger,
                                       int socket_scheduler);
t_datos_swap* inicializar_datos_swap(int socket_swap, t_logger* logger);
t_proceso* inicializar_proceso(u_int32_t pid, char* path_relativo,
                               char* scripts_basepath, t_logger* logger);
bool inicializar_ip_stick(t_datos_stick* datos_stick, int client_socket);
t_memoria_principal* inicializar_memoria_principal(
    int tamanio_total, t_allocation_strategy allocation_strategy);