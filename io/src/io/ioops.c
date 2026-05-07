#include "ioops.h"

bool io_tipo_stdin(t_modulo_io* sio, t_peticion_stdin peticion_stdin)
{
  t_list* paquete_recibido = recibir_paquete(sio->socket_io);

  // recibo el pquete y lo cargo
  if (paquete_recibido == NULL)
    return false;
  peticion_stdin.pid = *((uint32_t*)list_get(paquete_recibido, 0));
  peticion_stdin.tamanio_a_leer = *((uint32_t*)list_get(paquete_recibido, 1));
  peticion_stdin.direccion_logica = *((uint32_t*)list_get(paquete_recibido, 2));

  // Solicito el input por teclado
  printf("Proceso %d solicita que ingrese un input por teclado de tamaño %d\n",
         peticion_stdin.pid, peticion_stdin.tamanio_a_leer);
  fgets(peticion_stdin.buffer, peticion_stdin.tamanio_a_leer, stdin);

  // Armo el paquete para enviar a Kernel Scheduler
  t_paquete* paquete = crear_paquete();
  paquete->codigo_operacion = OP_RESPUESTA_STDIN;
  agregar_a_paquete(paquete, &peticion_stdin.pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, &peticion_stdin.direccion_logica,
                    sizeof(uint32_t));
  agregar_a_paquete(paquete, &peticion_stdin.tamanio_a_leer, sizeof(uint32_t));
  agregar_a_paquete(paquete, peticion_stdin.buffer,
                    peticion_stdin.tamanio_a_leer);

  // Envio el paquete y chequeo error
  bool envio_correcto = enviar_paquete(paquete, sio->socket_io);
  if (!envio_correcto)
  {
    log_error(sio->logger,
              "## Error al enviar la respuesta de IO a Kernel Scheduler");
    eliminar_paquete(paquete);
    return false;
  }
  eliminar_paquete(paquete);
  list_destroy_and_destroy_elements(paquete_recibido, free);
  return true;
}