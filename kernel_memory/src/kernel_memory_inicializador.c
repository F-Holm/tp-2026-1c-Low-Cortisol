#include "kernel_memory_inicializador.h"

#include <commons/log.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "commons/collections/list.h"
#include "kernel_memory_sv.h"

t_datos_kernel_mem* inicializar_datos_kernel_memory(int socket_kernel_memory,
                                                    t_log* logger)
{
  t_datos_kernel_mem* datos_kernel = malloc(sizeof(t_datos_kernel_mem));
  datos_kernel->socket_kernel_memory = socket_kernel_memory;
  datos_kernel->logger = logger;
  datos_kernel->sticks_conectados = list_create();
  datos_kernel->cpus_conectados = list_create();
  return datos_kernel;
}

t_datos_scheduler* inicializar_datos_scheduler(int socket_scheduler,
                                               t_log* logger)
{
  t_datos_scheduler* datos_scheduler = malloc(sizeof(t_datos_scheduler));
  datos_scheduler->socket_scheduler = socket_scheduler;
  datos_scheduler->logger = logger;
  return datos_scheduler;
}

t_datos_cpu* inicializar_datos_cpu(int socket_cpu, t_log* logger)
{
  t_datos_cpu* datos_cpu = malloc(sizeof(t_datos_cpu));
  datos_cpu->socket_cpu = socket_cpu;
  datos_cpu->logger = logger;
  datos_cpu->id = -1;
  return datos_cpu;
}

t_datos_stick* inicializar_datos_stick(int socket_stick, t_log* logger)
{
  t_datos_stick* datos_stick = malloc(sizeof(t_datos_stick));
  datos_stick->socket_stick = socket_stick;
  datos_stick->logger = logger;
  datos_stick->tamanio_stick = -1;
  datos_stick->puerto_stick = -1;
  return datos_stick;
}

t_datos_swap* inicializar_datos_swap(int socket_swap, t_log* logger)
{
  t_datos_swap* datos_swap = malloc(sizeof(t_datos_swap));
  datos_swap->socket_swap = socket_swap;
  datos_swap->logger = logger;
  return datos_swap;
}

bool inicializar_ip_stick(t_datos_stick* datos_stick, int client_socket)
{
  struct sockaddr addr;
  socklen_t addr_len = sizeof(addr);
  char ip_traducida[16];
  if (getpeername(client_socket, &addr, &addr_len) == 0)
  {
    // Casteamos a unsigned char para leer los bytes individuales
    unsigned char* datos = (unsigned char*)addr.sa_data;
    // Escribimos en el buffer con formato de IP
    // Los bytes 2,3,4,5 son la IP en la estructura genérica sockaddr
    sprintf(ip_traducida, "%d.%d.%d.%d", datos[2], datos[3], datos[4],
            datos[5]);
    strcpy(datos_stick->ip_memory_stick, ip_traducida);
    return true;
  }
  log_info(datos_stick->logger,
           "## NO SE HA PODIDO CONSEGUIR LA IP DE MEMORY_STICK");
  return false;
}
