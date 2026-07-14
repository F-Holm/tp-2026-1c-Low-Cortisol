#include "kernel_memory/inicializador.h"

t_datos_kernel_mem* inicializar_datos_kernel_memory(
    int socket_kernel_memory, char* scripts_basepath, int instruction_delay,
    int compaction_delay, int segment_max_size,
    t_allocation_strategy allocation_strategy, t_logger* logger)
{
  t_datos_kernel_mem* datos_kernel = malloc(sizeof(t_datos_kernel_mem));
  datos_kernel->socket_kernel_memory = socket_kernel_memory;
  datos_kernel->logger = logger;
  datos_kernel->socket_scheduler = -1;
  datos_kernel->scripts_basepath = scripts_basepath;
  datos_kernel->instruction_delay = instruction_delay;
  datos_kernel->compaction_delay = compaction_delay;
  datos_kernel->segment_max_size = segment_max_size;
  datos_kernel->allocation_strategy = allocation_strategy;
  datos_kernel->sticks_conectados = list_create();
  datos_kernel->cpus_conectados = list_create();
  datos_kernel->procesos = list_create();
  datos_kernel->memoria_principal =
      inicializar_memoria_principal(segment_max_size, allocation_strategy);
  datos_kernel->datos_swap = NULL;
  datos_kernel->mutex_procesos = malloc(sizeof(pthread_mutex_t));
  datos_kernel->mutex_lista_sockets = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(datos_kernel->mutex_procesos, NULL);
  pthread_mutex_init(datos_kernel->mutex_lista_sockets, NULL);
  return datos_kernel;
}

t_datos_scheduler* inicializar_datos_scheduler(
    int socket_scheduler, t_list* procesos, char* scripts_basepath,
    pthread_mutex_t* mutex_procesos, t_memoria_principal* memoria_principal,
    t_list* sticks_conectadas, pthread_mutex_t* mutex_sticks,
    t_datos_swap* datos_swap, t_logger* logger)
{
  t_datos_scheduler* datos_scheduler = malloc(sizeof(t_datos_scheduler));
  datos_scheduler->socket_scheduler = socket_scheduler;
  datos_scheduler->mutex_procesos = mutex_procesos;
  datos_scheduler->logger = logger;
  datos_scheduler->procesos = procesos;
  datos_scheduler->scripts_basepath = scripts_basepath;
  datos_scheduler->sticks_conectados = sticks_conectadas;
  datos_scheduler->mutex_lista_sockets = mutex_sticks;
  datos_scheduler->memoria_principal = memoria_principal;
  datos_scheduler->datos_swap = datos_swap;
  return datos_scheduler;
}

t_datos_cpu* inicializar_datos_cpu(int socket_cpu, t_list* procesos,
                                   pthread_mutex_t* mutex_procesos,
                                   int instruction_delay,
                                   t_memoria_principal* memoria_principal,
                                   t_logger* logger)
{
  t_datos_cpu* datos_cpu = malloc(sizeof(t_datos_cpu));
  datos_cpu->socket_cpu = socket_cpu;
  datos_cpu->procesos = procesos;
  datos_cpu->mutex_procesos = mutex_procesos;
  datos_cpu->instruction_delay = instruction_delay;
  datos_cpu->logger = logger;
  datos_cpu->id = -1;
  datos_cpu->memoria_principal = memoria_principal;
  return datos_cpu;
}

t_datos_stick* inicializar_datos_stick(int socket_stick, t_logger* logger,
                                       int socket_scheduler)
{
  t_datos_stick* datos_stick = malloc(sizeof(t_datos_stick));
  datos_stick->socket_stick = socket_stick;
  datos_stick->logger = logger;
  datos_stick->socket_scheduler = socket_scheduler;
  datos_stick->tamanio_stick = -1;
  datos_stick->puerto_stick = -1;
  return datos_stick;
}

static void inicializar_lista_bloques(t_datos_swap* datos_swap)
{
  int cantidad_bloques = datos_swap->tamanio_swap / datos_swap->tamanio_bloque;
  for (int i = 0; i < cantidad_bloques; i++)
  {
    t_datos_bloque* bloque = malloc(sizeof(t_datos_bloque));
    bloque->num_bloque = i;
    bloque->pid = -1;
    bloque->num_segmento = -1;
    bloque->num_bloque_del_segmento = -1;
    bloque->tamanio_segmento = -1;
    list_add(datos_swap->lista_bloques, bloque);
  }
}

t_datos_swap* inicializar_datos_swap(int socket_swap, t_logger* logger)
{
  t_datos_swap* datos_swap = malloc(sizeof(t_datos_swap));
  datos_swap->socket_swap = socket_swap;
  datos_swap->logger = logger;
  int a;
  t_envio_a_km* envio_km = (t_envio_a_km*)recibir_buffer(&a, socket_swap);
  datos_swap->tamanio_swap = envio_km->swap_size;
  datos_swap->tamanio_bloque = envio_km->block_size;
  free(envio_km);
  datos_swap->lista_bloques = list_create();
  inicializar_lista_bloques(datos_swap);
  return datos_swap;
}

int cantidad_instrucciones(FILE* f)
{
  int contador = 0;
  char linea[256];
  while (fgets(linea, sizeof(linea), f))
    contador++;
  rewind(f);
  return contador;
}

t_proceso* inicializar_proceso(u_int32_t pid, char* path_relativo,
                               char* scripts_basepath, t_logger* logger)
{
  t_proceso* proceso = malloc(sizeof(t_proceso));
  proceso->pid = pid;
  proceso->path_instrucciones = path_relativo;
  proceso->segmentos = list_create();
  memset(&proceso->registro, 0,
         sizeof(t_registros));  // pone todos los campos de registro en 0

  int largo = strlen(scripts_basepath) + strlen(path_relativo) + 2;
  char* path_completo = malloc(largo);
  snprintf(path_completo, largo, "%s/%s", scripts_basepath, path_relativo);
  FILE* f = fopen(path_completo, "r");
  if (f == NULL)
  {
    logger_error(logger, "## PID: %u - No se pudo abrir el archivo: %s", pid,
                 path_completo);
    free(path_completo);
    free(proceso);
    return NULL;
  }
  proceso->cant_instrucciones = cantidad_instrucciones(f);
  proceso->instrucciones = malloc(sizeof(char*) * proceso->cant_instrucciones);
  char linea[256];
  int i = 0;
  while (fgets(linea, sizeof(linea), f))
  {
    linea[strcspn(linea, "\n")] = '\0';
    proceso->instrucciones[i++] = strdup(linea);
  }
  fclose(f);
  free(path_completo);
  return proceso;
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
  logger_info(datos_stick->logger,
              "## NO SE HA PODIDO CONSEGUIR LA IP DE MEMORY_STICK");
  return false;
}

t_memoria_principal* inicializar_memoria_principal(
    int tamanio_maximo_segmento, t_allocation_strategy allocation_strategy)
{
  t_memoria_principal* memoria = malloc(sizeof(t_memoria_principal));
  memoria->tamanio_total = 0;
  memoria->tamanio_maximo_segmento = tamanio_maximo_segmento;
  memoria->mutex_memoria_principal = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(memoria->mutex_memoria_principal, NULL);
  memoria->segmentos = list_create();
  memoria->allocation_strategy = allocation_strategy;
  memoria->huecos = list_create();
  return memoria;
}