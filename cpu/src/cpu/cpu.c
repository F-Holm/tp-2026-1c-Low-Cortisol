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
  t_ip_puerto* nuevo_ip_puerto;

  while (1)
  {
    // ver como recivo la ip y el puerto para luego crear la conexion.
    lista_paquete = recibir_paquete(cpu->socket_kernel_memory);
    nuevo_ip_puerto = list_get(lista_paquete, 0);
    char ip_stick[16];
    strcpy(ip_stick, nuevo_ip_puerto->ip);
    int puerto_int = nuevo_ip_puerto->puerto;
    char puerto_stick[6];
    snprintf(puerto_stick, sizeof(puerto_stick), "%u", puerto_int);
    list_destroy_and_destroy_elements(lista_paquete, free);
    int nuevo_socket = crear_conexion(ip_stick, puerto_stick);

    log_info(cpu->logger, "Conectandose a memory stick con ip %s y puerto %s", ip_stick, puerto_stick);

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

    int* p_socket = malloc(sizeof(int));
    *p_socket = nuevo_socket;

    list_add(cpu->memory_sticks, p_socket);
  }

  list_destroy_and_destroy_elements(cpu->memory_sticks, (void*)iterator);
  return NULL;
}
