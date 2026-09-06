#include "swap/swap.h"

void cerrar_todo(t_modulo_swap* datos_swap, t_config* config)
{
  close(datos_swap->socket_swap);
  fclose(datos_swap->archivo_swap);
  log_destroy(datos_swap->logger);
  config_destroy(config);
}

static bool inicializar_archivo_swap(t_modulo_swap* datos_swap);

bool inicializar_configuracion(t_modulo_swap* datos_swap, t_config* config)
{
  char* log_levelstr = config_get_string_value(config, "LOG_LEVEL");

  datos_swap->ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
  datos_swap->puerto = config_get_string_value(config, "KERNEL_MEMORY_PUERTO");
  datos_swap->tamanio_swap = config_get_int_value(config, "SWAP_FILE_SIZE");
  datos_swap->tamanio_bloque = config_get_int_value(config, "BLOCK_SIZE");
  datos_swap->logger = log_create("swap.log", "SWAP", true,
                                  log_level_from_string(log_levelstr), false);
  if (datos_swap->logger == NULL)
  {
    config_destroy(config);
    return false;
  }
  datos_swap->swap_file_path =
      config_get_string_value(config, "SWAP_FILE_PATH");
  if (!inicializar_archivo_swap(datos_swap))
  {
    log_error(datos_swap->logger, "## Error al inicializar el archivo SWAP");
    cerrar_todo(datos_swap, config);
    return false;
  }

  return true;
}

bool iniciar_conexion(t_modulo_swap* datos_swap, t_config* config)
{
  datos_swap->socket_swap = crear_conexion(
      datos_swap->ip, datos_swap->puerto);  // Establezco conexión

  if (datos_swap->socket_swap == -1)
  {
    log_error(datos_swap->logger, "#ERROR DE CONEXION");
    cerrar_todo(datos_swap, config);
    return false;
  }
  log_info(datos_swap->logger, "## Conectado a Kernel Memory");

  // Handshake con Kernel Scheduler
  bool envio_correcto = enviar_handshake(MID_SWAP, datos_swap->socket_swap);
  if (!envio_correcto)
  {
    log_error(datos_swap->logger, "## Error en el Handshake con Kernel Memory");
    cerrar_todo(datos_swap, config);
    return false;
  }

  int recepcion_correcta = recibir_handshake(datos_swap->socket_swap);
  if (recepcion_correcta != MID_KERNEL_MEMORY)
  {
    log_error(datos_swap->logger, "## Error en el Handshake con Kernel Memory");
    cerrar_todo(datos_swap, config);
    return false;
  }
  log_info(datos_swap->logger, "Handshake exitoso con Kernel Memory");

  // Envio a memory el tamaño del swap y el tamaño de bloque
  t_envio_a_km* envio_km = malloc(sizeof(t_envio_a_km));
  int size_envio = sizeof(t_envio_a_km);
  envio_km->swap_size = datos_swap->tamanio_swap;
  envio_km->block_size = datos_swap->tamanio_bloque;
  envio_correcto = enviar_buffer(OP_INFO_SWAP, (void*)envio_km, size_envio,
                                 datos_swap->socket_swap);

  if (!envio_correcto)
  {
    log_error(datos_swap->logger, "## Error al enviar el paquete de SWAP");
    cerrar_todo(datos_swap, config);
    return false;
  }
  free(envio_km);

  return true;
}

static bool inicializar_archivo_swap(t_modulo_swap* datos_swap)
{
  FILE* archivo_swap = fopen(datos_swap->swap_file_path, "wb+");
  if (archivo_swap == NULL)
    return false;

  // Asignar tamanio e inicializar el archivo con ceros
  int file_descriptor = fileno(archivo_swap);
  if (ftruncate(file_descriptor, datos_swap->tamanio_swap) == -1)
  {
    fclose(archivo_swap);
    return false;
  }

  datos_swap->archivo_swap = archivo_swap;
  return true;
}

static void buscar_bloque(FILE* archivo_swap, int num_bloque,
                          int tamanio_bloque)
{
  fseek(archivo_swap, num_bloque * tamanio_bloque, 0);
}

void escribir_bloque(FILE* archivo_swap, int num_bloque, int tamanio_bloque,
                     char* contenido_a_escribir)
{
  buscar_bloque(archivo_swap, num_bloque, tamanio_bloque);
  fwrite(contenido_a_escribir, tamanio_bloque, 1, archivo_swap);
  fflush(archivo_swap);
}

void leer_bloque(FILE* archivo_swap, int num_bloque, int tamanio_bloque,
                 char* contenido_leido)
{
  buscar_bloque(archivo_swap, num_bloque, tamanio_bloque);
  if (fread(contenido_leido, tamanio_bloque, 1, archivo_swap) != 1)
    return;
}