#include "swap/agregados.h"

void cerrar_todo(t_modulo_swap* modulo_swap)
{
  close(modulo_swap->socket_swap);
  log_destroy(modulo_swap->logger);
  config_destroy(modulo_swap->config);
}

bool inicializar_configuracion(t_modulo_swap* modulo_swap)
{
  char* log_levelstr =
      config_get_string_value(modulo_swap->config, "LOG_LEVEL");

  modulo_swap->ip = config_get_string_value(modulo_swap->config, "IP");
  modulo_swap->puerto = config_get_string_value(modulo_swap->config, "PORT");
  modulo_swap->swap_size =
      config_get_int_value(modulo_swap->config, "SWAP_FILE_SIZE");
  modulo_swap->block_size =
      config_get_int_value(modulo_swap->config, "BLOCK_SIZE");

  modulo_swap->logger = log_create("swap.log", "SWAP", true,
                                   log_level_from_string(log_levelstr););
  if (modulo_swap->logger == NULL)
  {
    config_destroy(modulo_swap->config);
    return false;
  }
  return true;
}

bool iniciar_conexion(t_modulo_swap* modulo_swap)
{
  modulo_swap->socket_swap = crear_conexion(
      modulo_swap->ip, modulo_swap->puerto);  // Establezco conexión

  if (modulo_swap->socket_swap == -1)
  {
    log_error(modulo_swap->logger, "#ERROR DE CONEXION");
    cerrar_todo(modulo_swap);
    return false;
  }
  log_info(modulo_swap->logger, "## Conectado a Kernel Memory");

  // Handshake con Kernel Scheduler
  bool envio_correcto = enviar_handshake(MID_SWAP, modulo_swap->socket_swap);
  if (!envio_correcto)
  {
    log_error(modulo_swap->logger,
              "## Error en el Handshake con Kernel Memory");
    cerrar_todo(modulo_swap);
    return false;
  }

  int recepcion_correcta = recibir_handshake(modulo_swap->socket_swap);
  if (recepcion_correcta != MID_KERNEL_MEMORY)
  {
    log_error(modulo_swap->logger,
              "## Error en el Handshake con Kernel Memory");
    cerrar_todo(modulo_swap);
    return false;
  }
  log_info(modulo_swap->logger, "## Handshake exitoso con Kernel Memory");
  // Envio a memory el tamaño del swap y el tamaño de bloque
  t_paquete* paquete = crear_paquete();
  paquete->codigo_operacion = OP_PAQUETE;
  agregar_a_paquete(paquete, &modulo_swap->swap_size, sizeof(int));
  agregar_a_paquete(paquete, &modulo_swap->block_size, sizeof(int));
  envio_correcto = enviar_paquete(paquete, modulo_swap->socket_swap);

  if (!envio_correcto)
  {
    log_error(modulo_swap->logger, "## Error al enviar el paquete de SWAP");
    cerrar_todo(modulo_swap);
    return false;
  }
  eliminar_paquete(paquete);

  return true;
}
