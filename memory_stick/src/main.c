#include <arpa/inet.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"
#include "memory_stick/memory_stick.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_ms_recursos ms_recursos = {0};
  pthread_t thread_server_cpu;

  // args
  if (argc != 3)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];
  char* tamanio_str = argv[2];
  int tamanio = atoi(tamanio_str);
  if (tamanio <= 0)
    return EXIT_FAILURE;

  // Iniciar módulo
  if (!iniciar_modulo(&ms_recursos, archivo_config, tamanio_str))
  {
    if (ms_recursos.socket_server_cpu > 0)
      close(ms_recursos.socket_server_cpu);
    if (ms_recursos.socket_km > 0)
      close(ms_recursos.socket_km);
    if (ms_recursos.logger != NULL)
      log_destroy(ms_recursos.logger);
    if (ms_recursos.config != NULL)
      config_destroy(ms_recursos.config);
    return EXIT_FAILURE;
  }

  // Hilo para escuchar nuevas conexiones de CPUs
  t_datos_hilo_escucha datos_hilo_escucha;
  datos_hilo_escucha.socket_fd = ms_recursos.socket_server_cpu;
  datos_hilo_escucha.logger = ms_recursos.logger;
  pthread_create(&thread_server_cpu, NULL, hilo_escucha_cpu,
                 &datos_hilo_escucha);

  // Esperando Instrucciones del Kernel Memory
  while (true)
  {
    int op_code = recibir_operacion(ms_recursos.socket_km);
    char* buffer;

    if (op_code == OP_CODE_ERROR || op_code == -1)
      break;

    buffer = recibir_string(ms_recursos.socket_km);
    free(buffer);
  }

  // Liberar y Cerrar
  shutdown(ms_recursos.socket_server_cpu, SHUT_RDWR);
  pthread_join(thread_server_cpu, NULL);
  close(ms_recursos.socket_km);
  close(ms_recursos.socket_server_cpu);
  log_destroy(ms_recursos.logger);
  config_destroy(ms_recursos.config);
  return EXIT_SUCCESS;
}
