#include "kernel_memory/servidor.h"

void handshake(t_datos_kernel_mem* datos_kernel_memory, int client_socket)
{
  logger_info(datos_kernel_memory->logger, "Servidor a la espera de handshake");
  int identificador = recibir_handshake(client_socket);
  switch (identificador)
  {
    case MID_KERNEL_SCHEDULER:
    {
      if (!enviar_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        enviar_handshake_error(datos_kernel_memory->logger, client_socket,
                               "Kernel Scheduler");
        return;
      }
      logger_info(datos_kernel_memory->logger,
                  "## Kernel Scheduler Conectado - FD del socket: %i",
                  client_socket);
      t_datos_scheduler* datos_scheduler = inicializar_datos_scheduler(
          client_socket, datos_kernel_memory->procesos,
          datos_kernel_memory->scripts_basepath,
          datos_kernel_memory->mutex_procesos,
          datos_kernel_memory->memoria_principal,
          datos_kernel_memory->sticks_conectados,
          datos_kernel_memory->mutex_lista_sockets,
          datos_kernel_memory->logger);
      datos_kernel_memory->socket_scheduler = client_socket;
      empezar_escucha_scheduler(datos_scheduler);
    }
    break;

    case MID_CPU:
    {
      if (!enviar_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        enviar_handshake_error(datos_kernel_memory->logger, client_socket,
                               "CPU");
        return;
      }
      bool inicializar_correcto = true;
      logger_info(datos_kernel_memory->logger, "Se ha conectado una CPU!");
      t_datos_cpu* datos_cpu = inicializar_datos_cpu(
          client_socket, datos_kernel_memory->procesos,
          datos_kernel_memory->mutex_procesos,
          datos_kernel_memory->instruction_delay, datos_kernel_memory->memoria_principal, datos_kernel_memory->logger);
      inicializar_correcto = recibir_id_cpu(datos_cpu);
      enviar_buffer(OP_TAMANIO_MAX_SEG, &datos_kernel_memory->segment_max_size,
                    sizeof(int), datos_cpu->socket_cpu);
      agregar_conexion_cpu(datos_kernel_memory, datos_cpu);
      if (inicializar_correcto)
      {
        enviar_sticks_conectadas(datos_kernel_memory->sticks_conectados,
                                 datos_kernel_memory->mutex_lista_sockets,
                                 datos_cpu);

        empezar_escucha_cpu(datos_cpu);
      }
      else
      {
        error_incorrecta_inicializacion(datos_kernel_memory->logger,
                                        client_socket, "CPU");
      }
    }
    break;

    case MID_MEMORY_STICK:
    {
      if (!enviar_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        enviar_handshake_error(datos_kernel_memory->logger, client_socket,
                               "Memory Stick");
        return;
      }
      logger_info(datos_kernel_memory->logger,
                  "Se ha conectado una memory Stick!");
      bool inicializar_correcto = true;
      t_datos_stick* datos_stick =
          inicializar_datos_stick(client_socket, datos_kernel_memory->logger,
                                  datos_kernel_memory->socket_scheduler);
      inicializar_correcto = inicializar_ip_stick(datos_stick, client_socket);
      inicializar_correcto = recibir_tamanio_stick(datos_stick);
      inicializar_correcto = recibir_puerto_escucha_stick(datos_stick);
      agregar_conexion_stick(datos_kernel_memory, datos_stick);
      if (inicializar_correcto)
      {
        enviar_conexion_cpu(datos_stick, datos_kernel_memory->cpus_conectados);
        enviar_string(OP_NUEVO_MEMORY_STICK,
                      "Se ha conectado una nueva Memory Stick",
                      datos_kernel_memory->socket_scheduler);
        aniadir_memoria_total(datos_kernel_memory->memoria_principal,
                              datos_stick->tamanio_stick);
        empezar_escucha_stick(datos_stick);
      }
      else
      {
        error_incorrecta_inicializacion(datos_kernel_memory->logger,
                                        datos_stick->socket_stick,
                                        "Memory Stick");
      }
    }
    break;

    case MID_SWAP:
    {
      if (!enviar_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        enviar_handshake_error(datos_kernel_memory->logger, client_socket,
                               "SWAP");
        return;
      }
      logger_info(datos_kernel_memory->logger, "Se ha conectado el SWAP");
      t_datos_swap* datos_swap =
          inicializar_datos_swap(client_socket, datos_kernel_memory->logger);
      empezar_escucha_swap(datos_swap);
      break;
    }
    default:
      break;
  }
}

void accept_cliente(void* ptr)
{
  t_datos_kernel_mem* datos_kernel_memory = (t_datos_kernel_mem*)ptr;
  logger_info(datos_kernel_memory->logger,
              "Servidor a la espera de un cliente");
  int socket_cliente =
      esperar_cliente(datos_kernel_memory->socket_kernel_memory);
  logger_info(datos_kernel_memory->logger, "Se ha aceptado a un cliente!");
  handshake(datos_kernel_memory, socket_cliente);
}
