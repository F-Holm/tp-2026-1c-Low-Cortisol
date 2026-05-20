#include "cpu/cpu.h"

#include <commons/log.h>
#include <stdio.h>

#include "utils/kernel_memory_cpu.h"

void iniciar_hilo_kernel_memory(t_cpu* cpu)
{
  pthread_create(&cpu->hilos.kernel_memory_hilo, NULL, escuchar_kernel_memory,
                 cpu);

  log_info(cpu->logger, "Hilo de escucha de Kernel Memory iniciado");
}

void iniciar_hilo_kernel_scheduler(t_cpu* cpu)
{
  pthread_create(&cpu->hilos.kernel_scheduler_hilo, NULL, manejo_instrucciones,
                 cpu);

  log_info(cpu->logger, "Hilo de escucha de Kernel Memory iniciado");
}

void iterator_close_socket(void* value)
{
  close(*((int*)value));
  free(value);
}

bool verificar_argumentos(int argc, char** argv)
{
  if (argc < 3)
  {
    printf("Uso: %s [config] [id]\n", argv[0]);
    return false;
  }
  return true;
}

bool iniciar_modulo(t_cpu* cpu, char* path_config)
{
  // CONFIG Y LOGS
  cpu->config = config_create(path_config);

  t_log_level log_level =
      log_level_from_string(config_get_string_value(cpu->config, "LOG_LEVEL"));

  cpu->logger = log_create("cpu.log", cpu->id, true, log_level);

  if (cpu->config == NULL)
  {
    log_error(cpu->logger, "## No se pudo cargar el config");
    return false;
  }

  if (cpu->logger == NULL)
  {
    log_error(cpu->logger, "## No se pudo cargar el logger");
    return false;
  }

  log_info(cpu->logger, "Iniciando CPU %s", cpu->id);
  log_info(cpu->logger, "cpu->configcargado correctamente");

  cpu->memory_sticks = list_create();
  return true;
}

bool iniciar_conexion_scheduler(t_cpu* cpu)
{
  // CONEXION CON EL KERNEL SCHEDULER
  char* ip_kernel_scheduler =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_IP");

  char* puerto_kernel_scheduler =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_PUERTO");

  cpu->socket_kernel_scheduler =
      crear_conexion(ip_kernel_scheduler, puerto_kernel_scheduler);

  // Handshake con Kernel scheduler
  if (enviar_handshake(MID_CPU, cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger,
             "handshake correctamente enviado al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo en el envio del hanshake con kernel scheduler");
  }

  int id_modulo = recibir_handshake(cpu->socket_kernel_scheduler);
  if (id_modulo != MID_KERNEL_SCHEDULER)
  {
    log_error(cpu->logger,
              "## Error al recibir el Handshake con Kernel_scheduler");
    close(cpu->socket_kernel_scheduler);
    return false;
  }
  log_info(cpu->logger, "Handshake recibido con exitoso del Kernel scheduler");

  return true;
}

bool iniciar_conexion_kmemory(t_cpu* cpu)
{
  // CONEXION CON EL KERNEL MEMORY
  char* ip_kernel_memory =
      config_get_string_value(cpu->config, "KERNEL_MEMORY_IP");

  char* puerto_kernel_memory =
      config_get_string_value(cpu->config, "KERNEL_MEMORY_PUERTO");

  cpu->socket_kernel_memory =
      crear_conexion(ip_kernel_memory, puerto_kernel_memory);

  // Handshake con Kernel Memory
  if (enviar_handshake(MID_CPU, cpu->socket_kernel_memory))
  {
    log_info(cpu->logger, "handshake correctamente enviado al kernel memory");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo en el envio del hanshake con kernel memory");
  }

  int id_modulo = recibir_handshake(cpu->socket_kernel_memory);
  if (id_modulo != MID_KERNEL_MEMORY)
  {
    log_error(cpu->logger,
              "## Error al recibir el Handshake con Kernel_Memory");
    close(cpu->socket_kernel_memory);
    return false;
  }
  log_info(cpu->logger, "Handshake recibido del Kernel Memory");

  return true;
}

bool conexion_memory_stick(t_cpu* cpu, int nuevo_socket)
{
  // Handshake con memory stick
  if (!enviar_handshake(MID_CPU, nuevo_socket))
  {
    log_error(cpu->logger, "## Error en el Handshake con Memory stick");
    close(nuevo_socket);
    return false;
  }

  int id_modulo = recibir_handshake(nuevo_socket);
  if (id_modulo != MID_MEMORY_STICK)
  {
    log_error(cpu->logger, "## Error en el Handshake con Memory stick");
    close(nuevo_socket);
    return false;
  }
  log_info(cpu->logger, "## Handshake exitoso con Memory stick");

  return true;
}

bool manejar_paquete(t_cpu* cpu, t_list* lista_paquete, char ip_stick[16],
                     char puerto_stick[6])
{
  if (list_size(lista_paquete) != 2)
  {
    log_error(cpu->logger,
              "## Error en la recepción de la IP y puerto del Memory stick");
    log_error(cpu->logger, "## size: %d | expected size 2",
              list_size(lista_paquete));
    list_destroy_and_destroy_elements(lista_paquete, free);
    return false;
  }
  strcpy(ip_stick, list_get(lista_paquete, 0));
  strcpy(puerto_stick, list_get(lista_paquete, 1));
  list_destroy_and_destroy_elements(lista_paquete, free);
  log_info(cpu->logger, "IP: %s | Puerto: %s", ip_stick, puerto_stick);
  return true;
}

void* escuchar_kernel_memory(t_cpu* cpu)
{
 
  t_list* lista_paquete;

  while (1)
  {
    char ip_stick[16];
    char puerto_stick[6];
    int nuevo_socket;
    int codigo_operacion = recibir_operacion(cpu->socket_kernel_memory);
    if (codigo_operacion == OP_CODE_ERROR)
    {
      log_error(cpu->logger, "## Cerrando hilo");
      break;
    }
    else if (codigo_operacion != OP_PAQUETE)
    {
      log_error(cpu->logger, "## Tipo de operación no válido");
      continue;
    }

    lista_paquete = recibir_paquete(cpu->socket_kernel_memory);
    if (!manejar_paquete(cpu, lista_paquete, ip_stick, puerto_stick))
      continue;

    nuevo_socket = crear_conexion(ip_stick, puerto_stick);

    if (nuevo_socket <= 0)
    {
      log_error(cpu->logger, "## Error en la conexión con Memory stick");
      continue;
    }

    log_info(cpu->logger,
             "## Conectandose a memory stick con ip %s y puerto %s", ip_stick,
             puerto_stick);

    if (!conexion_memory_stick(cpu, nuevo_socket))
    {
      log_error(cpu->logger, "## Error en el handshake con Memory stick");
      continue;
    }

    if (!enviar_string(OP_ID_CPU, cpu->id, nuevo_socket))
    {
      log_error(cpu->logger, "## Error en el envío de ID con Memory stick");
      close(nuevo_socket);
      continue;
    }

    int* p_socket = malloc(sizeof(int));
    *p_socket = nuevo_socket;

    list_add(cpu->memory_sticks, p_socket);
  }

  list_destroy_and_destroy_elements(cpu->memory_sticks,
                                    (void*)iterator_close_socket);
  return NULL;
}


void manejo_instrucciones(t_cpu* cpu)
{
 
  t_contexto* contexto;
  uint32_t pid;

  while(true)
  {
    pid = recibir_pid_kernel_scheduler(cpu);

    if (!pedir_contexto_kernel_memory(cpu, pid));
      break;

    contexto = recibir_contexto_kernel_memory(cpu);

    ejecutar_ciclo_instruccion(cpu, pid, contexto);

    enviar_contexto_actualizado(cpu, contexto);

  }
  cerrar_modulo(cpu);
}


uint32_t recibir_pid_kernel_scheduler(t_cpu* cpu)
{
  int codigo_operacion = recibir_operacion(cpu->socket_kernel_scheduler);

  if (codigo_operacion == OP_CONTINUAR_PROCESO)
  {
    int size;
    void* buffer = recibir_buffer(&size, cpu->socket_kernel_scheduler);
    uint32_t pid = *(uint32_t*)buffer;
    free(buffer);

    log_info(cpu->logger, "## PID recibido: %u - Iniciando ciclo de instrucción", pid);
    return pid;
  }
  else 
  {
    cerrar_modulo(cpu);
  }
}

bool pedir_contexto_kernel_memory(t_cpu* cpu, uint32_t pid)
{
  return enviar_buffer(OP_PEDIR_CONTEXTO, &pid, sizeof(uint32_t), cpu->socket_kernel_memory);
}


t_contexto* recibir_contexto_kernel_memory(t_cpu* cpu)
{
  int codigo_operacion = recibir_operacion(cpu->socket_kernel_memory);

  if (codigo_operacion == OP_) // ??
  {
    int size;
    void* buffer = recibir_buffer(&size, cpu->socket_kernel_memory);
    t_contexto* contexto = malloc(sizeof(t_contexto));
    memcpy(contexto, buffer, size);
    free(buffer);
    return contexto;
  }
  else
  {
    
  }
}


void ejecutar_ciclo_instruccion(t_cpu* cpu, uint32_t pid, t_contexto* contexto)
{    
    bool seguir = true;
    while (seguir)
    {
      char* instruccion = etapa_fetch(cpu, pid, contexto->PC);
      log_info(cpu->logger, "## PID: %u - FETCH - Program Counter: %u", pid, contexto->PC); 


      etapa_decode(instruccion);

      seguir = execute(cpu, contexto, instruccion, pid);
      
      if (seguir)
          seguir = check_interrupt(cpu, pid);
      
      free(instruccion);
      contexto->PC ++;
    }
    
    // devolvés contexto actualizado y KernelMemory lo guarda
    enviar_contexto(cpu, contexto, pid);
    free(contexto);
}


char* etapa_fetch(t_cpu* cpu, uint32_t pid, uint32_t pc)
{
  pedir_instruccion_kernel_memory(cpu, pid, pc);

  return recibir_instruccion_kernel_memory(cpu);
}


void pedir_instruccion_kernel_memory(t_cpu* cpu, uint32_t pid, uint32_t pc) 
{
  t_paquete* paquete = crear_paquete(OP_SIGUIENTE_INSTRUCCION); 
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, &pc, sizeof(uint32_t));
  enviar_paquete(paquete, cpu->socket_kernel_memory);
  eliminar_paquete(paquete);
}


char* recibir_instruccion_kernel_memory(t_cpu* cpu)
{

}


bool etapa_decode(char* instruccion)
{

}


bool enviar_contexto_actualizado(t_cpu* cpu, t_contexto* contexto_actualizado)
{
  return enviar_buffer(OP_CONTEXTO_ACTUALIZADO, contexto_actualizado, sizeof(t_contexto), cpu->socket_kernel_memory);
}


void cerrar_modulo(t_cpu* cpu)
{
  if (cpu->socket_kernel_memory > 0)
    close(cpu->socket_kernel_memory);

  if (cpu->socket_kernel_scheduler > 0)
    close(cpu->socket_kernel_scheduler);

  if (cpu->logger != NULL)
    log_destroy(cpu->logger);

  if (cpu->config != NULL)
    config_destroy(cpu->config);
}
