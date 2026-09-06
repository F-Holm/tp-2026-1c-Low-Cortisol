#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"
#include "memory_stick/memory_stick.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_ms_recursos ms_recursos = {0};
  pthread_t thread_server_cpu;

  // args
  char* archivo_config = NULL;
  char* tamanio_str = NULL;
  int tamanio;
  if (!get_args(argc, argv, &archivo_config, &tamanio_str, &tamanio))
    return EXIT_FAILURE;

  // Iniciar módulo
  if (!iniciar_modulo(&ms_recursos, archivo_config, tamanio_str,
                      &thread_server_cpu))
  {
    cerrar_modulo_error(&ms_recursos);
    return EXIT_FAILURE;
  }

  // Esperando Instrucciones del Kernel Memory
  bool seguir_operando = true;
  while (seguir_operando)
  {
    int op_code = receive_op_code(ms_recursos.socket_km);
    switch (op_code)
    {
      case OP_MEMORY_STICK_READ:
      {
        log_info(ms_recursos.logger, "Recibiendo instrucción de lectura");
        t_list* packet = receive_packet(ms_recursos.socket_km);
        if (list_size(packet) != 2)
        {
          log_error(ms_recursos.logger,
                    "Cantidad de parametros para leer memoria invalida.");
          list_destroy_and_destroy_elements(packet, free);
          break;
        }
        int posicion_inicial = *(int*)list_get(packet, 0);
        int cantidad_bytes = *(int*)list_get(packet, 1);
        leer_memoria(&ms_recursos, posicion_inicial, cantidad_bytes,
                     ms_recursos.socket_km);
        log_info(ms_recursos.logger, "## Lectura de %d bytes", cantidad_bytes);
        list_destroy_and_destroy_elements(packet, free);
        break;
      }
      case OP_MEMORY_STICK_WRITE:
      {
        log_info(ms_recursos.logger, "Recibiendo instrucción de escritura");
        t_list* packet = receive_packet(ms_recursos.socket_km);
        if (list_size(packet) != 3)
        {
          log_error(ms_recursos.logger,
                    "Cantidad de parametros para escribir memoria invalida.");
          list_destroy_and_destroy_elements(packet, free);
          break;
        }
        int posicion_inicial = *(int*)list_get(packet, 0);
        char* bytes_a_escribir = (char*)list_get(packet, 1);
        int cantidad_bytes = *(int*)list_get(packet, 2);
        log_info(ms_recursos.logger,
                 "Escritura por parte del Kernel memory de %d bytes, desde "
                 "%d",
                 cantidad_bytes, posicion_inicial);
        escribir_memoria(&ms_recursos, posicion_inicial, bytes_a_escribir,
                         cantidad_bytes, ms_recursos.socket_km);
        log_info(ms_recursos.logger, "## Escritura de %d bytes",
                 cantidad_bytes);
        list_destroy_and_destroy_elements(packet, free);
        break;
      }
      default:
        seguir_operando = false;
        break;
    }
  }

  // Liberar y Cerrar
  cerrar_modulo(&ms_recursos, &thread_server_cpu);
  return EXIT_SUCCESS;
}
