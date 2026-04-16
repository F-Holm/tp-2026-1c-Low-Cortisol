#include <commons/config.h>
#include <commons/log.h>
#include <stdatomic.h>
#include <string.h>

#include "kernel_scheduler.h"
#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

int main(int argc, char* argv[])
{
  int server_CPU;
  int server_IO;
  int cliente_CPU;
  int cliente_IO;
  int socket_km;
  char* ip;
  char* puerto;
  char* valor;

  t_log* logger;
  t_config* config;
  atomic_bool seguir_operando = true;

  config = iniciar_config();
  logger = iniciar_logger(config);
  ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
  puerto = config_get_string_value(config, "KERNEL_MEMORY_PUERTO");
  valor = "Hola, soy el kernel scheduler";

  // conectar con kernel memory como cliente y loggear el resultado
  socket_km = crear_conexion(ip, puerto);
  if (socket_km <= 0)
  {
    log_error(logger, "Fallo la conexion con kernel memory en %s:%s", ip,
              puerto);
    terminar_programa(socket_km, logger, config);
    return 1;
  }
  else
  {
    log_info(logger, "Conexion establecida con kernel memory en %s:%s", ip,
             puerto);
  }

  // handshake con kernel memory y loggear el resultado
  enviar_handshake(MID_KERNEL_SCHEDULER, socket_km);
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

  //----------------------------------------------------------------------------------

  // iniciar servidor para CPU y IO
  server_CPU = iniciar_servidor();
  server_IO = iniciar_servidor();

  log_info(logger, "Servidor listo para recibir CPUs e IOs");
  cliente_CPU = esperar_cliente(server_CPU);
  cliente_IO = esperar_cliente(server_IO);

  // recibir operaciones de CPU y IO
  while (atomic_load(&seguir_operando))
  {
    int op_code = recibir_operacion(cliente);
    switch (op_code)
    {
        // agregar casos para cada operacion que se quiera recibir de la CPU y
        // IO
      case OP_CODE_ERROR:
        log_error(logger, "el cliente se desconecto. Terminando servidor");
        atomic_store(&seguir_operando, false);
        return EXIT_FAILURE;
      default:
        log_warning(logger, "Operacion desconocida.");
        break;
    }
  }

  // destruir variables y cerrar conexiones

  return EXIT_SUCCESS;
}
