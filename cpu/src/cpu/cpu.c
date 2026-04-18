#include "cpu/cpu.h"

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

void* escuchar_kernel_memory(void* arg)
{
  t_cpu* cpu = (t_cpu*)arg;
  while (1)
  {
    // ver como recivo la ip y el puerto para luego crear la conexion.
    char* ip_stick = "127.0.0.1";
    char* puerto_stick = "22342";

    int nuevo_socket = crear_conexion(ip_stick, puerto_stick);

    // Handshake con memory stick
    enviar_handshake(MID_CPU, nuevo_socket);

    int id_modulo = recibir_handshake(nuevo_socket);
    if (id_modulo != MID_MEMORY_STICK)
    {
      log_error(cpu->logger, "## Error en el Handshake con Memory stick,");
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
