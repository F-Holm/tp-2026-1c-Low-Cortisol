#include "cpu/cpu.h"

#include <commons/log.h>
#include <stdio.h>

#include "utils/kernel_memory_cpu.h"

void iniciar_hilo(void* arg)
{
  t_cpu* cpu = (t_cpu*)arg;
  pthread_create(&cpu->hilos.kernel_memory_hilo, NULL, escuchar_kernel_memory,
                 cpu);
}

void iterator(void* value)
{
  close(*((int*)value));
  free(value);
}

void asignador(void* value)
{
}

void* escuchar_kernel_memory(void* arg)
{
  t_cpu* cpu = (t_cpu*)arg;
  t_list* lista_paquete;

  while (1)
  {
    char ip_stick[16];
    char puerto_stick[6];
    int codigo_operacion = recibir_operacion(cpu->socket_kernel_memory);
    if (codigo_operacion != OP_PAQUETE)
    {
      log_error(cpu->logger, "## Error en el tipo de operación");
      log_destroy(cpu->logger);
      config_destroy(cpu->config);
      return NULL;
    }
    lista_paquete = recibir_paquete(cpu->socket_kernel_memory);

    if (list_size(lista_paquete) != 2)
    {
      log_error(cpu->logger,
                "## Error en la recepción de la IP y puerto del Memory stick");
      log_error(cpu->logger, "## size: %d | expected size 2",
                list_size(lista_paquete));
      log_destroy(cpu->logger);
      config_destroy(cpu->config);
      list_destroy_and_destroy_elements(lista_paquete, free);
      return NULL;
    }
    strcpy(ip_stick, list_get(lista_paquete, 0));
    strcpy(puerto_stick, list_get(lista_paquete, 1));
    list_destroy_and_destroy_elements(lista_paquete, free);

    int nuevo_socket = crear_conexion(ip_stick, puerto_stick);
    if (nuevo_socket <= 0)
    {
      log_error(cpu->logger, "## Error en la conexión con Memory stick");
      log_destroy(cpu->logger);
      config_destroy(cpu->config);
      return NULL;
    }

    log_info(cpu->logger,
             "## Conectandose a memory stick con ip %s y puerto %s", ip_stick,
             puerto_stick);

    // Handshake con memory stick
    enviar_handshake(MID_CPU, nuevo_socket);

    int id_modulo = recibir_handshake(nuevo_socket);
    if (id_modulo != MID_MEMORY_STICK)
    {
      log_error(cpu->logger, "## Error en el Handshake con Memory stick");
      close(nuevo_socket);
      log_destroy(cpu->logger);
      config_destroy(cpu->config);
      return NULL;
    }
    log_info(cpu->logger, "## Handshake exitoso con Memory stick");

    enviar_string(OP_ID_CPU, cpu->id, nuevo_socket);

    int* p_socket = malloc(sizeof(int));
    *p_socket = nuevo_socket;

    list_add(cpu->memory_sticks, p_socket);
  }

  list_destroy_and_destroy_elements(cpu->memory_sticks, (void*)iterator);
  return NULL;
}
