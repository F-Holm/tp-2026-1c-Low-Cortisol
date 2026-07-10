#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "swap/swap.h"
#include "utils/client.h"
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
    int op_code = recibir_operacion(datos_swap.socket_swap);
    switch (op_code)
    {
    case OP_ESCRIBIR_DISCO:
      t_list* paquete = recibir_paquete(datos_swap.socket_swap);
        if (list_size(paquete) != 2)
        {
          logger_error(datos_swap.logger,
                       "Cantidad de parametros para escribir en disco invalida.");
          break;
        }
        int numero_bloque = *(int*)list_get(paquete, 0);
        char* contenido_a_escribir = (char*)list_get(paquete, 1);
        escribir_bloque(datos_swap.archivo_swap, numero_bloque, datos_swap.tamanio_bloque, contenido_a_escribir);
        enviar_string(OP_DISCO_ESCRITO, "", datos_swap.socket_swap);
        list_destroy_and_destroy_elements(paquete, free);
      break;

    case OP_LEER_DISCO:
      int a;
      int* num_bloque = (int*)recibir_buffer(&a, datos_swap.socket_swap);
      if(num_bloque == NULL)
      {
        logger_error(datos_swap.logger,
                     "Error al recibir el numero del bloque a leer.");
        break;
      }
      char* contenido_leido = malloc(datos_swap.tamanio_bloque);
      leer_bloque(datos_swap.archivo_swap, *num_bloque, datos_swap.tamanio_bloque, contenido_leido);
      enviar_string(OP_DISCO_LEIDO, contenido_leido, datos_swap.socket_swap);
      free(num_bloque);
      break;

    default:
      seguir_operando = false;
      break;
    }
  }
  // Liberar y Cerrar
  cerrar_todo(&datos_swap, config);
  return EXIT_SUCCESS;
}
