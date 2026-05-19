#include "kernel_memory/protocolo.h"

#include <commons/collections/list.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/configurador.h"
#include "utils/msg.h"

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

void agregar_conexion_stick(t_datos_kernel_mem* datos_kernel_memory,
                            t_datos_stick* datos_stick)
{
  pthread_mutex_lock(datos_kernel_memory->mutex_lista_sockets);
  list_add(datos_kernel_memory->sticks_conectados, datos_stick);
  pthread_mutex_unlock(datos_kernel_memory->mutex_lista_sockets);
  return;
}

void agregar_conexion_cpu(t_datos_kernel_mem* datos_kernel_memory,
                          t_datos_cpu* datos_cpu)
{
  pthread_mutex_lock(datos_kernel_memory->mutex_lista_sockets);
  list_add(datos_kernel_memory->cpus_conectados, datos_cpu);
  pthread_mutex_unlock(datos_kernel_memory->mutex_lista_sockets);
  return;
}

void enviar_sticks_conectadas(t_list* sticks_conectados,
                              pthread_mutex_t* mutex_lista_sockets,
                              t_datos_cpu* datos_cpu)
{
  pthread_mutex_lock(mutex_lista_sockets);
  t_list* copia_sticks = list_duplicate(sticks_conectados);
  pthread_mutex_unlock(mutex_lista_sockets);

  int total_sticks = list_size(copia_sticks);

  for (int i = 0; i < total_sticks; i++)
  {
    t_paquete* paquete = crear_paquete(OP_PAQUETE);
    t_datos_stick* stick_actual = (t_datos_stick*)list_get(copia_sticks, i);

    char puerto[6];
    snprintf(puerto, sizeof(puerto), "%u", stick_actual->puerto_stick);

    agregar_a_paquete(paquete, stick_actual->ip_memory_stick, sizeof(char[16]));
    agregar_a_paquete(paquete, puerto, sizeof(puerto));

    enviar_paquete(paquete, datos_cpu->socket_cpu);
    eliminar_paquete(paquete);
  }
  list_destroy(copia_sticks);
}

void enviar_conexion_cpu(t_datos_stick* datos_stick, t_list* cpus_conectados)
{
  if (list_is_empty(cpus_conectados))
  {
    return;
  }
  t_paquete* paquete = crear_paquete(OP_PAQUETE);

  agregar_a_paquete(paquete, datos_stick->ip_memory_stick, sizeof(char[16]));

  char puerto[6];
  snprintf(puerto, sizeof(puerto), "%u", datos_stick->puerto_stick);
  agregar_a_paquete(paquete, puerto, sizeof(puerto));

  for (int i = 0; i < list_size(cpus_conectados); i++)
  {
    t_datos_cpu* cpu_actual = (t_datos_cpu*)list_get(cpus_conectados, i);
    enviar_paquete(paquete, cpu_actual->socket_cpu);
  }
  eliminar_paquete(paquete);
}

u_int32_t calcular_memoria_total(t_list* sticks_conectados,
                                 pthread_mutex_t* mutex_lista_sockets)
{
  u_int32_t total = 0;
  for (int i = 0; i < list_size(sticks_conectados); i++)
  {
    pthread_mutex_lock(mutex_lista_sockets);
    t_datos_stick* stick_actual =
        (t_datos_stick*)list_get(sticks_conectados, i);
    pthread_mutex_unlock(mutex_lista_sockets);
    total += stick_actual->tamanio_stick;
  }
  return total;
}

void enviar_tamanio_disponible_scheduler(int socket_scheduler,
                                         t_list* sticks_conectados,
                                         pthread_mutex_t* mutex_lista_sockets,
                                         t_log* logger)
{
  t_paquete* paquete = crear_paquete(OP_TAMANIO_TOTAL_MEMORIA);
  u_int32_t tamanio_total =
      calcular_memoria_total(sticks_conectados, mutex_lista_sockets);
  agregar_a_paquete(paquete, &tamanio_total, sizeof(u_int32_t));
  enviar_paquete(paquete, socket_scheduler);
  eliminar_paquete(paquete);
}
