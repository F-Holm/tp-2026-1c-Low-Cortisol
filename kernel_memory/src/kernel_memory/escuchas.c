#include "kernel_memory/escuchas.h"

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
            datos_scheduler->logger);

        aniadir_lista_mtx(datos_scheduler->procesos,
                         datos_scheduler->mutex_procesos, proceso);

        logger_info(datos_scheduler->logger, "## PID: %ls - Proceso Creado",
                    pid);

        free(pid);
        list_clean(paquete);
        list_destroy(paquete);
        break;
      }
      case OP_SYSCALL_MEM_ALLOC:
      {
        logger_info(datos_scheduler->logger, "Llego una syscall de MEM_ALLOC");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)recibir_buffer(
            &a, datos_scheduler->socket_scheduler);
        break;
      }
      case OP_SYSCALL_MEM_FREE:
      {
        logger_info(datos_scheduler->logger, "Llego una syscall de MEM_FREE");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)recibir_buffer(
            &a, datos_scheduler->socket_scheduler);
        break;
      }
      case OP_PETICION_IO_STDIN:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una syscall de PETICION_IO_STDIN");
        t_list* paquete_stdin = recibir_paquete(
            datos_scheduler
                ->socket_scheduler);  // RECIBE STRUCT DE PETICION STDOUT
        enviar_string(OP_OK, "OK", datos_scheduler->socket_scheduler);
        break;
      }
      case OP_PETICION_IO_STDOUT:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una syscall de PETICION_IO_STDOUT");
        int a;
        t_peticion_stdout* stdout = (t_peticion_stdout*)recibir_buffer(
            &a, datos_scheduler->socket_scheduler);
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

        logger_info(datos_cpu->logger,
                    "## PID: %u - Obtener instrucción: %u - Instrucción: %s",
                    pid, pc, instruccion);
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
        logger_info(datos_cpu->logger, "## PID: %u - Obtener contexto", *pid);
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
        logger_info(datos_swap->logger, "Llego un paquete de la memory stick");
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
        logger_info(datos_stick->logger, "Llego un paquete de la memory stick");
        // comunicaciones
        break;
      case OP_MEMORY_STICK_LEIDO:
        char* lectura = recibir_string(datos_stick->socket_stick);
        logger_info(datos_stick->logger, "Se ha leido de la memory stick: %s",
                    lectura);
        enviar_string(OP_RESPUESTA_STDOUT, lectura,
                      datos_stick->socket_scheduler);
        free(lectura);
        break;
      case OP_MEMORY_STICK_ESCRITO:
        char* buffer = recibir_string(datos_stick->socket_stick);
        free(buffer);
        logger_info(datos_stick->logger, "Se ha escrito en la memory stick");
        enviar_string(OP_RESPUESTA_STDIN, "", datos_stick->socket_scheduler);
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