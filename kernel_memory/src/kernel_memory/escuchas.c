#include "kernel_memory/escuchas.h"

#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdlib.h>

#include "configurador.h"
#include "kernel_memory/estructuras.h"
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
  liberar_datos_scheduler(datos_scheduler);
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