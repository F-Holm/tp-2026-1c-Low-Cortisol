#include "kernel_memory/sv.h"

#include <commons/log.h>
#include <pthread.h>
#include <stdlib.h>

#include "commons/collections/list.h"
#include "commons/config.h"
#include "kernel_memory/inicializador.h"
#include "utils/kernel_memory_cpu.h"
#include "utils/msg.h"
#include "utils/server.h"

t_log* iniciar_logger(t_config* config)
{
  return log_create(
      "kernel_memory.log", "kernel_memory", true,
      log_level_from_string(config_get_string_value(config, "LOG_LEVEL")));
}
t_config* iniciar_config(char* path)
{
  return config_create(path);
}

void terminar_comunicacion(int socket_cliente)
{
  close(socket_cliente);
}

void* escucha_scheduler(void* ptr)
{
  t_datos_scheduler* datos_scheduler = (t_datos_scheduler*)ptr;
  bool conexion_estable = true;
  while (conexion_estable)
  {
    switch (recibir_operacion(datos_scheduler->socket_scheduler))
    {
      case OP_PAQUETE:
        // t_list* paquete = recibir_paquete(datos_scheduler->socket_scheduler);
        log_info(datos_scheduler->logger,
                 "Llego un paquete de la memory stick");
        // comunicaciones
        break;
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;

      default:
        break;
    }
  }
  terminar_comunicacion(datos_scheduler->socket_scheduler);
  return NULL;
}

void* escucha_cpu(void* ptr)
{
  t_datos_cpu* datos_cpu = (t_datos_cpu*)ptr;
  bool conexion_estable = true;
  while (conexion_estable)
  {
    switch (recibir_operacion(datos_cpu->socket_cpu))
    {
      case OP_PAQUETE:
        // t_list* paquete = recibir_paquete(datos_cpu->socket_cpu);
        log_info(datos_cpu->logger, "Llego un paquete de la memory stick");
        // comunicaciones
        break;
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;

      default:
        break;
    }
  }
  terminar_comunicacion(datos_cpu->socket_cpu);
  return NULL;
}

void* escucha_swap(void* ptr)
{
  t_datos_swap* datos_swap = (t_datos_swap*)ptr;
  bool conexion_estable = true;
  while (conexion_estable)
  {
    switch (recibir_operacion(datos_swap->socket_swap))
    {
      case OP_PAQUETE:
        // t_list* paquete = recibir_paquete(datos_swap->socket_swap);
        log_info(datos_swap->logger, "Llego un paquete de la memory stick");
        // comunicaciones
        break;
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;

      default:
        break;
    }
  }
  terminar_comunicacion(datos_swap->socket_swap);
  return NULL;
}

void* escucha_stick(void* ptr)
{
  t_datos_stick* datos_stick = (t_datos_stick*)ptr;
  bool conexion_estable = true;
  while (conexion_estable)
  {
    switch (recibir_operacion(datos_stick->socket_stick))
    {
      case OP_PAQUETE:
        // t_list* paquete = recibir_paquete(datos_stick->socket_stick);
        log_info(datos_stick->logger, "Llego un paquete de la memory stick");
        // comunicaciones
        break;
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;

      default:
        break;
    }
  }
  terminar_comunicacion(datos_stick->socket_stick);
  return NULL;
}

void empezar_escucha_scheduler(t_datos_scheduler* datos_scheduler)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_scheduler, &datos_scheduler);
  pthread_detach(hilo_escucha);
}

void empezar_escucha_cpu(t_datos_cpu* datos_cpu)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_cpu, &datos_cpu);
  pthread_detach(hilo_escucha);
}

void empezar_escucha_stick(t_datos_stick* datos_stick)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_stick, &datos_stick);
  pthread_detach(hilo_escucha);
}

void empezar_escucha_swap(t_datos_swap* datos_swap)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_swap, &datos_swap);
  pthread_detach(hilo_escucha);
}

bool recibir_id_cpu(t_datos_cpu* datos_cpu)
{
  if (recibir_operacion(datos_cpu->socket_cpu) == OP_ID_CPU)
  {
    char* id_cpu = recibir_string(datos_cpu->socket_cpu);
    log_info(datos_cpu->logger, "## CPU %s Conectada", id_cpu);
    datos_cpu->id = atoi(id_cpu);
    free(id_cpu);
    return true;
  }
  else
  {
    log_info(datos_cpu->logger,
             "No se pudo realizar la conexion con el CPU ya que no se "
             "envio la operacion de ID");
    terminar_comunicacion(datos_cpu->socket_cpu);
    return false;
  }
  return false;
}

bool recibir_tamanio_stick(t_datos_stick* datos_stick)
{
  if (recibir_operacion(datos_stick->socket_stick) == OP_TAMANIO_MEMORIA)
  {
    char* tamanio = recibir_string(datos_stick->socket_stick);
    log_info(datos_stick->logger, "## Memory Stick de %s bytes Conectada",
             tamanio);
    datos_stick->tamanio_stick = atoi(tamanio);
    free(tamanio);
    return true;
  }
  else
  {
    log_info(datos_stick->logger,
             "No se pudo realizar la conexion con la stick ya que no se "
             "envio la operacion de tamaño");

    terminar_comunicacion(datos_stick->socket_stick);
    return false;
  }
  return false;
}

bool recibir_puerto_escucha_stick(t_datos_stick* datos_stick)
{
  if (recibir_operacion(datos_stick->socket_stick) == OP_PUERTO)
  {
    char* puerto = recibir_string(datos_stick->socket_stick);
    log_info(datos_stick->logger, "## Puerto de Memory Stick recibido %s",
             puerto);
    datos_stick->puerto_stick = atoi(puerto);
    free(puerto);
    return true;
  }
  else
  {
    log_info(datos_stick->logger,
             "No se pudo realizar la conexion con la stick ya que no se "
             "envio la operacion puerto");
    terminar_comunicacion(datos_stick->socket_stick);
    return false;
  }
  return false;
}

void agregar_coexion_stick(t_datos_kernel_mem* datos_kernel_memory,
                           t_datos_stick* datos_stick)
{
  list_add(datos_kernel_memory->sticks_conectados, datos_stick);
  return;
}

void enviar_sticks_conectadas(t_datos_kernel_mem* datos_kernel_memory,
                              t_datos_cpu* datos_cpu)
{
  for (int i = 0; i < list_size(datos_kernel_memory->sticks_conectados); i++)
  {
    t_paquete* paquete = crear_paquete();
    t_datos_stick* stick_actual =
        (t_datos_stick*)list_get(datos_kernel_memory->sticks_conectados, i);
    t_ip_puerto* nuevo_ip_puerto;
    nuevo_ip_puerto->puerto = stick_actual->puerto_stick;
    strcpy(nuevo_ip_puerto->ip, stick_actual->ip_memory_stick);
    agregar_a_paquete(paquete, &nuevo_ip_puerto, sizeof(t_ip_puerto));
    enviar_paquete(paquete, datos_cpu->socket_cpu);
  }
}

void enviar_conexion_cpus(t_datos_stick* datos_stick, t_list* cpus_conectados)
{
  t_paquete* paquete = crear_paquete();
  // 1. Reservamos memoria para la estructura
  t_ip_puerto* nuevo_ip_puerto = malloc(sizeof(t_ip_puerto));

  // 2. Asignamos los campos (Usamos -> porque es un puntero)
  nuevo_ip_puerto->puerto = datos_stick->puerto_stick;
  strcpy(nuevo_ip_puerto->ip, datos_stick->ip_memory_stick);

  // 3. Lo agregamos al paquete (ya es un puntero, no hace falta el &)

  agregar_a_paquete(paquete, nuevo_ip_puerto, sizeof(t_ip_puerto));
  // 1. Recorremos la lista de CPUs conectadas
  for (int i = 0; i < list_size(cpus_conectados); i++)
  {
    // 2. Obtenemos la CPU actual
    t_datos_cpu* cpu_actual = (t_datos_cpu*)list_get(cpus_conectados, i);
    // 3. Enviamos los datos
    enviar_paquete(paquete, cpu_actual->socket_cpu);
  }
  free(nuevo_ip_puerto);
}

void handshake(t_datos_kernel_mem* datos_kernel_memory, int client_socket)
{
  log_info(datos_kernel_memory->logger, "Servidor a la espera de handshake");
  int identificador = recibir_handshake(client_socket);
  switch (identificador)
  {
    case MID_KERNEL_SCHEDULER:
    {
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      log_info(datos_kernel_memory->logger,
               "## Kernel Scheduler Conectado - FD del socket: %i",
               client_socket);
      t_datos_scheduler* datos_scheduler = inicializar_datos_scheduler(
          client_socket, datos_kernel_memory->logger);
      empezar_escucha_scheduler(datos_scheduler);
    }
    break;

    case MID_CPU:
    {
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      bool inicializar_correcto = true;
      log_info(datos_kernel_memory->logger, "Se ha conectado una CPU!");
      t_datos_cpu* datos_cpu =
          inicializar_datos_cpu(client_socket, datos_kernel_memory->logger);
      inicializar_correcto = recibir_id_cpu(datos_cpu);
      if (inicializar_correcto)
      {
        enviar_sticks_conectadas(datos_kernel_memory, datos_cpu);

        empezar_escucha_cpu(datos_cpu);
      }
      else
      {
        terminar_comunicacion(client_socket);
      }
    }
    break;

    case MID_MEMORY_STICK:
    {
      // FALTA ENVIAR LA STICK CUANDO SE CONECTA A LA CPU
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      log_info(datos_kernel_memory->logger,
               "Se ha conectado una memory Stick!");
      bool inicializar_correcto = true;
      t_datos_stick* datos_stick =
          inicializar_datos_stick(client_socket, datos_kernel_memory->logger);
      inicializar_correcto = inicializar_ip_stick(datos_stick, client_socket);
      inicializar_correcto = recibir_tamanio_stick(datos_stick);
      inicializar_correcto = recibir_puerto_escucha_stick(datos_stick);
      agregar_coexion_stick(datos_kernel_memory, datos_stick);
      if (inicializar_correcto)
      {
        enviar_conexion_cpus(datos_stick, datos_kernel_memory->cpus_conectados);
        empezar_escucha_stick(datos_stick);
      }
      else
      {
        log_info(datos_kernel_memory->logger,
                 "Se ha terminado la conexion con una memory Stick ya que no "
                 "se pudo inicializar correectametne");
        terminar_comunicacion(datos_stick->socket_stick);
      };
    }
    break;

    case MID_SWAP:
    {
      enviar_handshake(MID_KERNEL_MEMORY, client_socket);
      log_info(datos_kernel_memory->logger, "Se ha conectado el SWAP!");
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
  log_info(datos_kernel_memory->logger, "Servidor a la espera de un cliente");
  int socket_cliente =
      esperar_cliente(datos_kernel_memory->socket_kernel_memory);
  log_info(datos_kernel_memory->logger, "Se ha aceptado a un cliente!");
  handshake(datos_kernel_memory, socket_cliente);
}

int main(int argc, char* argv[])
{
  if (argc != 2)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];

  t_config* config = iniciar_config(archivo_config);
  t_log* logger = iniciar_logger(config);
  int socket_kernel_memory =
      iniciar_servidor(config_get_string_value(config, "PUERTO_KERNEL_MEMORY"));

  t_datos_kernel_mem* datos_kernel =
      inicializar_datos_kernel_memory(socket_kernel_memory, logger);

  while (true)
  {
    accept_cliente(datos_kernel);
  }

  free(datos_kernel);

  return 0;
}
