#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "swap/swap.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_modulo_swap datos_swap;

  if (argc != 2)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];
  t_config* config = config_create(archivo_config);
  if (config == NULL)
    return EXIT_FAILURE;

  if (!inicializar_configuracion(&datos_swap, config))
  {
    return EXIT_FAILURE;
  }

  if (!iniciar_conexion(&datos_swap, config))
  {
    return EXIT_FAILURE;
  }
  // Esperando Instrucciones del Kernel Memory
  bool seguir_operando = true;
  while (seguir_operando)
  {
    int op_code = receive_op_code(datos_swap.socket_swap);
    switch (op_code)
    {
      case OP_DISK_WRITE:
        t_list* packet = receive_packet(datos_swap.socket_swap);
        if (list_size(packet) != 2)
        {
          log_error(datos_swap.logger,
                    "Cantidad de parametros para escribir en disco invalida.");
          break;
        }
        int numero_bloque = *(int*)list_get(packet, 0);
        char* contenido_a_escribir = (char*)list_get(packet, 1);
        escribir_bloque(datos_swap.archivo_swap, numero_bloque,
                        datos_swap.tamanio_bloque, contenido_a_escribir);
        send_string(OP_DISK_WRITE_DONE, "", datos_swap.socket_swap);
        list_destroy_and_destroy_elements(packet, free);
        log_info(datos_swap.logger, "## Escritura de bloque: <%d>",
                 numero_bloque);
        break;

      case OP_DISK_READ:
        int a;
        int* num_bloque = (int*)receive_buffer(&a, datos_swap.socket_swap);
        if (num_bloque == NULL)
        {
          log_error(datos_swap.logger,
                    "Error al recibir el numero del bloque a leer.");
          break;
        }
        char* contenido_leido = malloc(datos_swap.tamanio_bloque);
        leer_bloque(datos_swap.archivo_swap, *num_bloque,
                    datos_swap.tamanio_bloque, contenido_leido);
        send_buffer(OP_DISK_READ_DONE, contenido_leido,
                    datos_swap.tamanio_bloque, datos_swap.socket_swap);
        log_info(datos_swap.logger, "## Lectura de bloque: <%d>", *num_bloque);
        free(num_bloque);
        free(contenido_leido);
        break;

      default:
        seguir_operando = false;
        break;
    }
  }
  // Liberar y Cerrar
  log_info(datos_swap.logger, "Cerrando Swap");
  cerrar_todo(&datos_swap, config);
  return EXIT_SUCCESS;
}
