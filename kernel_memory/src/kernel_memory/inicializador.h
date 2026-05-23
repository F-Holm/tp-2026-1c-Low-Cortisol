#include <commons/log.h>
#include <pthread.h>

#include "commons/collections/list.h"
#include "kernel_memory/estructuras.h"

t_datos_kernel_mem* inicializar_datos_kernel_memory(
    int socket_kernel_memory, char* scripts_basepath, int instruction_delay,
    int compaction_delay, int segment_max_size, int allocation_strategy,
    t_log* logger);
t_datos_scheduler* inicializar_datos_scheduler(int socket_scheduler,
                                               t_list* procesos,
                                               char* scripts_basepath,
                                               pthread_mutex_t* mutex_procesos,
                                               pthread_mutex_t* mutex_logger,
                                               t_log* logger);
t_datos_cpu* inicializar_datos_cpu(int socket_cpu, t_list* procesos,
                                   pthread_mutex_t* mutex_procesos,
                                   pthread_mutex_t* mutex_logger,
                                   int instruction_delay, t_log* logger);
t_datos_stick* inicializar_datos_stick(int socket_stick, t_log* logger,
                                       pthread_mutex_t* mutex_logger);
t_datos_swap* inicializar_datos_swap(int socket_swap, t_log* logger,
                                     pthread_mutex_t* mutex_logger);
t_proceso* inicializar_proceso(u_int32_t pid, char* path_relativo,
                               char* scripts_basepath, t_log* logger,
                               pthread_mutex_t* mutex_logger);
bool inicializar_ip_stick(t_datos_stick* datos_stick, int client_socket);
