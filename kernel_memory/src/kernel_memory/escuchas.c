#include "kernel_memory/escuchas.h"

#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdlib.h>

#include "configurador.h"
#include "kernel_memory/estructuras.h"
#include "kernel_memory/inicializador.h"
#include "kernel_memory/liberador.h"
#include "utils/msg.h"

void* escucha_scheduler(void* ptr)
{
  t_datos_scheduler* datos_scheduler = (t_datos_scheduler*)ptr;
  bool conexion_estable = true;
  while (conexion_estable)
  {
    switch (recibir_operacion(datos_scheduler->socket_scheduler))
    {
      case OP_NUEVO_PROCESO:
        t_list* paquete = recibir_paquete(datos_scheduler->socket_scheduler);
        char* path_relativo = list_get(paquete, 0);
        u_int32_t* pid = list_get(paquete, 1);

        t_proceso* proceso = inicializar_proceso(
            *pid, path_relativo, datos_scheduler->scripts_basepath,
            datos_scheduler->logger);

        pthread_mutex_lock(datos_scheduler->mutex_procesos);
        list_add(datos_scheduler->procesos, proceso);
        pthread_mutex_unlock(datos_scheduler->mutex_procesos);

        log_info(datos_scheduler->logger, "## PID: %ls - Proceso Creado", pid);

        free(pid);
        list_clean(paquete);
        list_destroy(paquete);
        break;
      case OP_SYSCALL_MEM_ALLOC:
        log_info(datos_scheduler->logger, "Llego una syscall de MEM_ALLOC");
        break;

      case OP_SYSCALL_MEM_FREE:
        log_info(datos_scheduler->logger, "Llego una syscall de MEM_FREE");
        break;
      case OP_PETICION_IO_STDIN:
        log_info(datos_scheduler->logger,
                 "Llego una syscall de PETICION_IO_STDIN");
        t_list* paquete_stdin = recibir_paquete(
            datos_scheduler
                ->socket_scheduler);  // RECIBE STRUCT DE PETICION STDOUT
        enviar_string(OP_OK, "OK", datos_scheduler->socket_scheduler);
        break;
      case OP_PETICION_IO_STDOUT:
        log_info(datos_scheduler->logger,
                 "Llego una syscall de PETICION_IO_STDOUT");
        t_list* paquete_stdout = recibir_paquete(
            datos_scheduler
                ->socket_scheduler);  // RECIBE STRUCT DE PETICION STDIN
        enviar_string(OP_OK, "OK", datos_scheduler->socket_scheduler);
        break;

      case OP_CODE_ERROR:
        conexion_estable = false;
        break;

      default:
        break;
    }
  }
  liberar_datos_scheduler(datos_scheduler);
  return NULL;
}

t_proceso* buscar_proceso(t_datos_cpu* datos_cpu, uint32_t pid)
{
  for (int i = 0; i < list_size(datos_cpu->procesos); i++)
  {
    pthread_mutex_lock(datos_cpu->mutex_procesos);
    t_proceso* proceso = list_get(datos_cpu->procesos, i);
    pthread_mutex_unlock(datos_cpu->mutex_procesos);
    if (proceso->pid == pid)
      return proceso;
  }
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
      case OP_SIGUIENTE_INSTRUCCION:
        t_list* paquete = recibir_paquete(datos_cpu->socket_cpu);
        uint32_t pid = *(uint32_t*)list_get(paquete, 0);
        uint32_t pc = *(uint32_t*)list_get(paquete, 1);

        t_proceso* proceso = buscar_proceso(datos_cpu, pid);
        char* instruccion = proceso->instrucciones[pc];

        log_info(datos_cpu->logger,
                 "## PID: %u - Obtener instrucción: %u - Instrucción: %s", pid,
                 pc, instruccion);

        usleep(datos_cpu->instruction_delay * 1000);
        enviar_string(OP_ENVIAR_INSTRUCCION, instruccion,
                      datos_cpu->socket_cpu);
        break;
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;

      default:
        break;
    }
  }
  liberar_datos_cpu(datos_cpu);
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
  liberar_datos_swap(datos_swap);
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
  liberar_datos_stick(datos_stick);
  return NULL;
}

void empezar_escucha_scheduler(t_datos_scheduler* datos_scheduler)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_scheduler, datos_scheduler);
  pthread_detach(hilo_escucha);
}

void empezar_escucha_cpu(t_datos_cpu* datos_cpu)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_cpu, datos_cpu);
  pthread_detach(hilo_escucha);
}

void empezar_escucha_stick(t_datos_stick* datos_stick)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_stick, datos_stick);
  pthread_detach(hilo_escucha);
}

void empezar_escucha_swap(t_datos_swap* datos_swap)
{
  pthread_t hilo_escucha;
  pthread_create(&hilo_escucha, NULL, escucha_swap, datos_swap);
  pthread_detach(hilo_escucha);
}