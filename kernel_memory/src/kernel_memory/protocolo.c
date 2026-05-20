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
  pthread_mutex_lock(&datos_kernel_memory->mutex_lista_sockets);
  list_add(datos_kernel_memory->sticks_conectados, datos_stick);
  pthread_mutex_unlock(&datos_kernel_memory->mutex_lista_sockets);
  return;
}

void agregar_conexion_cpu(t_datos_kernel_mem* datos_kernel_memory,
                          t_datos_cpu* datos_cpu)
{
  pthread_mutex_lock(&datos_kernel_memory->mutex_lista_sockets);
  list_add(datos_kernel_memory->cpus_conectados, datos_cpu);
  pthread_mutex_unlock(&datos_kernel_memory->mutex_lista_sockets);
  return;
}

void enviar_sticks_conectadas(t_datos_kernel_mem* datos_kernel_memory,
                              t_datos_cpu* datos_cpu)
{
  pthread_mutex_lock(&datos_kernel_memory->mutex_lista_sockets);
  int total_sticks = list_size(datos_kernel_memory->sticks_conectados);

  for (int i = 0; i < total_sticks; i++)
  {
    t_paquete* paquete = crear_paquete();
    t_datos_stick* stick_actual =
        (t_datos_stick*)list_get(datos_kernel_memory->sticks_conectados, i);

    char puerto[6];
    snprintf(puerto, sizeof(puerto), "%u", stick_actual->puerto_stick);

    agregar_a_paquete(paquete, stick_actual->ip_memory_stick, sizeof(char[16]));
    agregar_a_paquete(paquete, puerto, sizeof(puerto));

    enviar_paquete(paquete, datos_cpu->socket_cpu);
    eliminar_paquete(paquete);
  }
  pthread_mutex_unlock(&datos_kernel_memory->mutex_lista_sockets);
}

void enviar_conexion_cpu(t_datos_stick* datos_stick, t_list* cpus_conectados)
{
  if (list_is_empty(cpus_conectados))
  {
    return;
  }
  t_paquete* paquete = crear_paquete();

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
