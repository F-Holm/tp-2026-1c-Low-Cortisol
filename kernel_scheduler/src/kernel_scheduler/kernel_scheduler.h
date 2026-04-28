#include <commons/config.h>
#include <commons/log.h>

t_log* iniciar_logger(t_config* config);
t_config* iniciar_config(char* path);
void iniciar_modulo(t_kScheduler_recursos* kScheduler_recursos, char* archivo_config);
bool conectar_kernel_memory(t_kScheduler_recursos* kScheduler_recursos);
bool handshake_kernel_memory(t_kScheduler_recursos* kScheduler_recursos);
void iniciar_servidor_cpu_io(t_kScheduler_recursos* kScheduler_recursos, t_datos_hilo_escucha* datos_hilo_escucha);
void cerrar_modulo(t_kScheduler_recursos* kScheduler_recursos);

typedef struct
{
  int socket_fd;
  t_log* logger;
} t_datos_hilo_escucha;

typedef struct
{
  t_config* config;
  t_log* logger;
  char* ip;
  char* puerto;
  char* puerto_servidor;
  int socket_km;
  int server;
  pthread_t thread_server;
} t_kScheduler_recursos;