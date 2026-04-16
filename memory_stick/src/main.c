#include <arpa/inet.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"
#include "memory_stick/memory_stick.h"
#include "utils/msg.h"

typedef struct
{
  int socket_fd;
  t_log* logger;
} t_datos_hilo_escucha;

typedef struct
{
  int* socket_fd;
  t_list* lista_sockets;
  pthread_mutex_t* mutex_lista_sockets;
  atomic_bool* todos_terminaron;
  pthread_cond_t* cond_fin_hilo_escucha;
} t_datos_hilo_cpu;

void* hilo_escucha_cpu(void* datos_hilo_escucha_void);
void* manejar_cliente_cpu(void* datos_hilo_cpu_void);

int main(int argc, char* argv[])
{
  t_config_vars config_vars;
  t_config* config;
  t_log* logger;
  int socket_km;
  int socket_server_cpu;
  uint16_t puerto_server_cpu;
  pthread_t thread_server_cpu;
  t_log_level log_level;
  atomic_bool seguir_operando = true;

  // Config
  config = config_create("memory_stick.config");
  if (config == NULL)
    return EXIT_FAILURE;
  read_confir_ms(config, &config_vars);

  // Logger
  log_level = log_level_from_string(config_vars.log_level);
  logger = log_create("memory_stick.log", "memory_stick", true, log_level);
  if (logger == NULL)
  {
    config_destroy(config);
    return EXIT_FAILURE;
  }

  // Socket Kernel Memory
  socket_km = conectar_km(config_vars.ip_km, config_vars.puerto_km);
  if (socket_km <= 0)
  {
    log_error(logger, "## Error de conexión al Kernel Memory");
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;
  }
  log_info(logger, "## Conectado a Kernel Memory");

  // Handshake con Kernel Memory
  enviar_handshake(MID_MEMORY_STICK, socket_km);
  int id_modulo = recibir_handshake(socket_km);
  if (id_modulo != MID_KERNEL_MEMORY)
  {
    log_error(logger, "## Error en el Handshake con Kernel Memory");
    close(socket_km);
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;
  }
  log_info(logger, "## Handshake exitoso con Kernel Memory");

  // Enviar puerto del servidor a Memory Kernel
  socket_server_cpu = create_server_cpu();
  puerto_server_cpu = get_puerto_cpu(socket_server_cpu);
  enviar_puerto_server_ms_km(socket_km, puerto_server_cpu);

  // Hilo para escuchar nuevas conexiones de CPUs
  t_datos_hilo_escucha datos_hilo_escucha;
  datos_hilo_escucha.socket_fd = socket_server_cpu;
  datos_hilo_escucha.logger = logger;
  pthread_create(&thread_server_cpu, NULL, hilo_escucha_cpu,
                 &datos_hilo_escucha);

  // Esperando Instrucciones del Kernel Memory
  while (atomic_load(&seguir_operando))
  {
    int op_code = recibir_operacion(socket_km);
    char* buffer;
    switch (op_code)
    {
      case OP_CODE_ERROR:
        atomic_store(&seguir_operando, false);
        break;
      default:
        buffer = recibir_string(socket_km);
        free(buffer);
        break;
    }
  }

  // Liberar y Cerrar
  shutdown(socket_server_cpu, SHUT_RDWR);
  pthread_join(thread_server_cpu, NULL);
  liberar_conexion(socket_km);
  liberar_conexion(socket_server_cpu);
  log_destroy(logger);
  config_destroy(config);
  return EXIT_SUCCESS;
}

void* hilo_escucha_cpu(void* datos_hilo_escucha_void)
{
  int socket_espera_cpu =
      ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->socket_fd;
  t_log* logger = ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->logger;

  t_list* lista_sockets = list_create();
  pthread_mutex_t mutex_lista_sockets;
  pthread_mutex_init(&mutex_lista_sockets, NULL);
  atomic_bool todos_terminaron = true;
  pthread_cond_t cond_fin_hilo_escucha;
  pthread_cond_init(&cond_fin_hilo_escucha, NULL);

  while (true)
  {
    int* socket_cpu = malloc(sizeof(int));
    *socket_cpu = esperar_cliente(socket_espera_cpu);
    if (socket_cpu <= 0)
      break;

    log_info(logger, "## Conexión exitosa con Kernel Memory");

    // Handshake con CPU
    int id_modulo = recibir_handshake(*socket_cpu);
    if (id_modulo != MID_KERNEL_MEMORY)
    {
      close(*socket_cpu);
      log_error(logger, "## Error en el Handshake con CPU");
      continue;
    }
    enviar_handshake(MID_MEMORY_STICK, *socket_cpu);
    log_info(logger, "## Handshake exitoso con Kernel Memory");

    // Obtener ID
    int codigo_operacion = recibir_operacion(*socket_cpu);
    if (codigo_operacion != OP_IP)
    {
      close(*socket_cpu);
      log_error(logger, "## Error en la recepción del ID de la CPU");
      continue;
    }
    char* id_cpu = recibir_string(*socket_cpu);
    log_info(logger, "## CPU %s Conectada", id_cpu);
    free(id_cpu);

    // Iniciar y liberar hilo
    t_datos_hilo_cpu* datos_hilo_cpu = malloc(sizeof(t_datos_hilo_cpu));
    datos_hilo_cpu->socket_fd = socket_cpu;
    datos_hilo_cpu->lista_sockets = lista_sockets;
    datos_hilo_cpu->mutex_lista_sockets = &mutex_lista_sockets;
    datos_hilo_cpu->todos_terminaron = &todos_terminaron;
    datos_hilo_cpu->cond_fin_hilo_escucha = &cond_fin_hilo_escucha;
    pthread_mutex_lock(&mutex_lista_sockets);
    if (list_is_empty(lista_sockets))
      atomic_store(&todos_terminaron, false);
    list_add(lista_sockets, socket_cpu);
    pthread_mutex_unlock(&mutex_lista_sockets);
    pthread_t hilo_cpu;
    pthread_create(&hilo_cpu, NULL, manejar_cliente_cpu, datos_hilo_cpu);
    pthread_detach(hilo_cpu);
  }

  log_info(logger, "## Cerrando servidor");
  pthread_mutex_lock(&mutex_lista_sockets);
  while (!atomic_load(&todos_terminaron))
    pthread_cond_wait(&cond_fin_hilo_escucha, &mutex_lista_sockets);
  pthread_mutex_unlock(&mutex_lista_sockets);
  list_destroy(lista_sockets);
  return NULL;
}

void* manejar_cliente_cpu(void* datos_hilo_cpu_void)
{
  int* socket_cpu = ((t_datos_hilo_cpu*)datos_hilo_cpu_void)->socket_fd;
  t_list* lista_sockets =
      ((t_datos_hilo_cpu*)datos_hilo_cpu_void)->lista_sockets;
  pthread_mutex_t* mutex_lista_sockets = ((t_datos_hilo_cpu*)datos_hilo_cpu_void)
      ->mutex_lista_sockets;
  atomic_bool* todos_terminaron = ((t_datos_hilo_cpu*)datos_hilo_cpu_void)
      ->todos_terminaron;
  pthread_cond_t* cond_fin_hilo_escucha = ((t_datos_hilo_cpu*)datos_hilo_cpu_void)
      ->cond_fin_hilo_escucha;
  free(datos_hilo_cpu_void);

  while (true)
  {
    int operacion = recibir_operacion(*socket_cpu);
    if (operacion == OP_CODE_ERROR)
      break;
    char* buffer = recibir_string(*socket_cpu);
    free(buffer);
  }

  // Liberar conexión y eliminar socket de la lista
  liberar_conexion(*socket_cpu);
  pthread_mutex_lock(mutex_lista_sockets);
  list_remove_element(lista_sockets, socket_cpu);
  if (list_is_empty(lista_sockets))
  {
    atomic_store(todos_terminaron, true);
    pthread_cond_signal(cond_fin_hilo_escucha);
  }
  pthread_mutex_unlock(mutex_lista_sockets);
  free(socket_cpu);
  return NULL;
}
