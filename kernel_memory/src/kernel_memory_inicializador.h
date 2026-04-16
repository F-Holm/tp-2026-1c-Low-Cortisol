#include "commons/collections/list.h"
#include <commons/log.h>
#include "kernel_memory_sv.h"


t_datos_kernel_mem* inicializar_datos_kernel_memory(int socket_kernel_memory,t_log* logger);
t_datos_scheduler* inicializar_datos_scheduler(int socket_scheduler, t_log* logger);
t_datos_cpu* inicializar_datos_cpu(int socket_cpu, t_log* logger);
t_datos_stick* inicializar_datos_stick(int socket_stick, int tamanio_stick,t_log* logger);
t_datos_swap* inicializar_datos_swap(int socket_swap, t_log* logger);