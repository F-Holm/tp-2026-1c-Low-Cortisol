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
      {
        t_list* paquete = recibir_paquete(datos_scheduler->socket_scheduler);
        char* path_relativo = list_get(paquete, 0);
        u_int32_t* pid = list_get(paquete, 1);

        t_proceso* proceso = inicializar_proceso(
            *pid, path_relativo, datos_scheduler->scripts_basepath,
            datos_scheduler->logger, datos_scheduler->mutex_logger);

        pthread_mutex_lock(datos_scheduler->mutex_procesos);
        list_add(datos_scheduler->procesos, proceso);
        pthread_mutex_unlock(datos_scheduler->mutex_procesos);

        pthread_mutex_lock(datos_scheduler->mutex_logger);
        log_info(datos_scheduler->logger, "## PID: %ls - Proceso Creado", pid);
        pthread_mutex_unlock(datos_scheduler->mutex_logger);

        free(pid);
        list_clean(paquete);
        list_destroy(paquete);
        break;
      }
      case OP_SYSCALL_MEM_ALLOC:
      {
        pthread_mutex_lock(datos_scheduler->mutex_logger);
        log_info(datos_scheduler->logger, "Llego una syscall de MEM_ALLOC");
        pthread_mutex_unlock(datos_scheduler->mutex_logger);
        break;
      }
      case OP_SYSCALL_MEM_FREE:
      {
        pthread_mutex_lock(datos_scheduler->mutex_logger);
        log_info(datos_scheduler->logger, "Llego una syscall de MEM_FREE");
        pthread_mutex_unlock(datos_scheduler->mutex_logger);
        break;
      }
      case OP_PETICION_IO_STDIN:
      {
        pthread_mutex_lock(datos_scheduler->mutex_logger);
        log_info(datos_scheduler->logger,
                 "Llego una syscall de PETICION_IO_STDIN");
        pthread_mutex_unlock(datos_scheduler->mutex_logger);
        t_list* paquete_stdin = recibir_paquete(
            datos_scheduler
                ->socket_scheduler);  // RECIBE STRUCT DE PETICION STDOUT
        enviar_string(OP_OK, "OK", datos_scheduler->socket_scheduler);
        break;
      }
      case OP_PETICION_IO_STDOUT:
      {
        pthread_mutex_lock(datos_scheduler->mutex_logger);
        log_info(datos_scheduler->logger,
                 "Llego una syscall de PETICION_IO_STDOUT");
        pthread_mutex_unlock(datos_scheduler->mutex_logger);
        t_list* paquete_stdout = recibir_paquete(
            datos_scheduler
                ->socket_scheduler);  // RECIBE STRUCT DE PETICION STDIN
        enviar_string(OP_OK, "OK", datos_scheduler->socket_scheduler);
        break;
      }
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
  t_proceso* resultado = NULL;
  pthread_mutex_lock(datos_cpu->mutex_procesos);
  for (int i = 0; i < list_size(datos_cpu->procesos); i++)
  {
    t_proceso* proceso = list_get(datos_cpu->procesos, i);
    if (proceso->pid == pid)
      resultado = proceso;
  }
  pthread_mutex_unlock(datos_cpu->mutex_procesos);
  return resultado;
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
      {
        t_list* paquete = recibir_paquete(datos_cpu->socket_cpu);
        uint32_t pid = *(uint32_t*)list_get(paquete, 0);
        uint32_t pc = *(uint32_t*)list_get(paquete, 1);
        list_destroy_and_destroy_elements(paquete, free);

        t_proceso* proceso = buscar_proceso(datos_cpu, pid);
        char* instruccion = proceso->instrucciones[pc];

        pthread_mutex_lock(datos_cpu->mutex_logger);
        log_info(datos_cpu->logger,
                 "## PID: %u - Obtener instrucción: %u - Instrucción: %s", pid,
                 pc, instruccion);
        pthread_mutex_unlock(datos_cpu->mutex_logger);

        usleep(datos_cpu->instruction_delay * 1000);
        enviar_string(OP_ENVIAR_INSTRUCCION, instruccion,
                      datos_cpu->socket_cpu);
        break;
      }
      case OP_PEDIR_CONTEXTO:
      {
        int a;
        uint32_t* pid = (uint32_t*)recibir_buffer(&a, datos_cpu->socket_cpu);
        t_proceso* proceso = buscar_proceso(datos_cpu, *pid);
        pthread_mutex_lock(datos_cpu->mutex_logger);
        log_info(datos_cpu->logger, "## PID: %u - Obtener contexto", *pid);
        pthread_mutex_unlock(datos_cpu->mutex_logger);
        free(pid);
        usleep(datos_cpu->instruction_delay * 1000);
        enviar_buffer(OP_ENVIAR_CONTEXTO, &proceso->contexto,
                      sizeof(t_contexto), datos_cpu->socket_cpu);
        break;
      }
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
        pthread_mutex_lock(datos_swap->mutex_logger);
        log_info(datos_swap->logger, "Llego un paquete de la memory stick");
        pthread_mutex_unlock(datos_swap->mutex_logger);
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
        pthread_mutex_lock(datos_stick->mutex_logger);
        log_info(datos_stick->logger, "Llego un paquete de la memory stick");
        pthread_mutex_unlock(datos_stick->mutex_logger);
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