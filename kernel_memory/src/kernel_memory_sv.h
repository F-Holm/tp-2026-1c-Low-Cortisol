typedef struct 
{
  int socket_kernel_memory;
  t_log* logger;
}t_datos_kernel_mem;

typedef struct 
{
  int socket_scheduler;
  t_log* logger;
}t_datos_scheduler;

typedef struct 
{
  int socket_cpu;
  t_log* logger;
}t_datos_cpu;

typedef struct 
{
  int socket_swap;
  t_log* logger;
}t_datos_swap;

typedef struct 
{
  int socket_stick;
  t_log* logger;
}t_datos_stick;