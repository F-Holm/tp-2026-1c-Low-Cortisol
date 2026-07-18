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

        logger_info(datos_scheduler->logger, "## PID: %d  - Proceso Creado",
                    *pid);
        list_destroy_and_destroy_elements(paquete, free);
        enviar_string(OP_PROCESO_INICIADO, "Proceso creado",
                      datos_scheduler->socket_scheduler);
        break;
      }
      case OP_SYSCALL_MEM_ALLOC:
      {
        logger_info(datos_scheduler->logger, "Llego una syscall de MEM_ALLOC");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)recibir_buffer(
            &a, datos_scheduler->socket_scheduler);

        if (syscall->tamanio >
            datos_scheduler->memoria_principal->tamanio_maximo_segmento)
        {
          logger_info(
              datos_scheduler->logger,
              "La syscall de MEM_ALLOC no se pudo realizar ya que el tamaño "
              "solicitado es mayor al tamaño máximo de segmento");
          enviar_string(
              OP_TAMANIO_SEGMENTO_EXCEDIDO,
              "Tamaño solicitado es mayor al tamaño máximo de segmento",
              datos_scheduler->socket_scheduler);
        }
        else
        {
          crear_segmento(syscall->id_segmento, syscall->pid, syscall->tamanio,
                         datos_scheduler->memoria_principal,
                         datos_scheduler->socket_scheduler,
                         datos_scheduler->logger);
        }
        free(syscall);
        break;
      }
      case OP_SYSCALL_MEM_FREE:
      {
        logger_info(datos_scheduler->logger, "Llego una syscall de MEM_FREE");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)recibir_buffer(
            &a, datos_scheduler->socket_scheduler);
        eliminar_segmento(syscall->id_segmento, syscall->pid,
                          datos_scheduler->memoria_principal,
                          datos_scheduler->logger);
        logger_info(datos_scheduler->logger, "se ha eliminado correctamente");
        free(syscall);
        enviar_string(OP_MEMORIA_LIBERADA, "Memoria liberada",
                      datos_scheduler->socket_scheduler);
        break;
      }
      case OP_PETICION_IO_STDIN:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una syscall de PETICION_IO_STDIN");
        t_list* paquete_stdin =
            recibir_paquete(datos_scheduler->socket_scheduler);
        t_peticion_stdin* peticion_stdin =
            (t_peticion_stdin*)list_get(paquete_stdin, 0);
        char* string_escribir = (char*)list_get(paquete_stdin, 1);
        int dir_fisica = traducir_direccion_logica(
            peticion_stdin->pid, peticion_stdin->direccion_logica,
            peticion_stdin->tamanio_a_leer, datos_scheduler->memoria_principal,
            datos_scheduler->logger);
        if (dir_fisica == -1)
        {
          enviar_string(OP_RESPUESTA_STDIN, "Segmentation Fault",
                        datos_scheduler->socket_scheduler);
          list_destroy_and_destroy_elements(paquete_stdin, free);
          break;
        }
        /*logger_info(datos_scheduler->logger,
                    "## PID: %u - Escritura - "
                    "Dir. Fisica: %u - Tamaño: %d",
                    peticion_stdout->pid, dir_fisica,
                    peticion_stdout->tamanio_a_escribir);*/
        if (!escribir_en_sticks(
                peticion_stdin->pid, dir_fisica, peticion_stdin->tamanio_a_leer,
                string_escribir, datos_scheduler->sticks_conectados,
                datos_scheduler->mutex_lista_sockets, datos_scheduler->logger,
                datos_scheduler->socket_scheduler))
        {
          logger_error(datos_scheduler->logger, "Error al escribir en sticks");
          conexion_estable = false;
          list_destroy_and_destroy_elements(paquete_stdin, free);
          break;
        }
        enviar_string(OP_RESPUESTA_STDIN, "Memoria Escrita",
                      datos_scheduler->socket_scheduler);
        logger_info(datos_scheduler->logger, "Peticion STDIN finalizada");
        list_destroy_and_destroy_elements(paquete_stdin, free);
        break;
      }
      case OP_PETICION_IO_STDOUT:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una syscall de PETICION_IO_STDOUT");
        int size;
        t_peticion_stdout* peticion_stdout = (t_peticion_stdout*)recibir_buffer(
            &size, datos_scheduler->socket_scheduler);
        int dir_fisica = traducir_direccion_logica(
            peticion_stdout->pid, peticion_stdout->direccion_logica,
            peticion_stdout->tamanio_a_escribir,
            datos_scheduler->memoria_principal, datos_scheduler->logger);
        logger_info(datos_scheduler->logger,
                    "## PID: %u - Lectura - "
                    "Dir. Fisica: %u - Tamaño: %d",
                    peticion_stdout->pid, dir_fisica,
                    peticion_stdout->tamanio_a_escribir);
        if (dir_fisica == -1)
        {
          enviar_string(OP_RESPUESTA_STDOUT, "Segmentation Fault",
                        datos_scheduler->socket_scheduler);
          free(peticion_stdout);
          break;
        }
        char* buffer = leer_de_sticks(
            dir_fisica, peticion_stdout->tamanio_a_escribir,
            datos_scheduler->sticks_conectados,
            datos_scheduler->mutex_lista_sockets, datos_scheduler->logger,
            datos_scheduler->socket_scheduler);
        if (buffer == NULL)
        {
          enviar_string(OP_RESPUESTA_STDOUT, "Error lectura stick",
                        datos_scheduler->socket_scheduler);
          conexion_estable = false;
          free(peticion_stdout);
          break;
        }
        enviar_string(OP_RESPUESTA_STDOUT, buffer,
                      datos_scheduler->socket_scheduler);
        free(buffer);
        free(peticion_stdout);
        logger_info(datos_scheduler->logger, "Peticion STDOUT finalizada");
        break;
      }
      case OP_TERMINAR_PROCESO:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una solicitud de TERMINAR_PROCESO");
        int a;
        uint32_t* pid =
            (uint32_t*)recibir_buffer(&a, datos_scheduler->socket_scheduler);
        t_proceso* proceso_a_terminar = buscar_proceso(
            datos_scheduler->procesos, datos_scheduler->mutex_procesos, *pid);
        if (proceso_a_terminar != NULL)
        {
          pthread_mutex_lock(datos_scheduler->mutex_procesos);
          list_remove_element(datos_scheduler->procesos, proceso_a_terminar);
          pthread_mutex_unlock(datos_scheduler->mutex_procesos);
          logger_info(datos_scheduler->logger, "Proceso con PID %u terminado",
                      *pid);
          for (int i = 0; i < list_size(proceso_a_terminar->segmentos); i++)
          {
            t_segmento* segmento = list_get(proceso_a_terminar->segmentos, i);
            eliminar_segmento(segmento->id, proceso_a_terminar->pid,
                              datos_scheduler->memoria_principal,
                              datos_scheduler->logger);
          }
          liberar_proceso(proceso_a_terminar);
        }
        else
        {
          logger_info(datos_scheduler->logger,
                      "No se encontró el proceso con PID %u para terminar",
                      *pid);
        }
        free(pid);
        break;
      }
      case OP_PEDIR_MEMORIA_DISPONIBLE:
      {
        free(recibir_string(datos_scheduler->socket_scheduler));
        logger_info(
            datos_scheduler->logger,
            "Se requiere la memoria disponible por parte del scheduler");

        int tamanio = calcular_espacio_libre(
            datos_scheduler->memoria_principal->huecos,
            datos_scheduler->memoria_principal->mutex_memoria_principal,
            datos_scheduler->logger);
        logger_info(datos_scheduler->logger, "espacio libre calculado");
        enviar_buffer(OP_MEMORIA_DISPONIBLE, &tamanio, sizeof(int),
                      datos_scheduler->socket_scheduler);
        logger_info(datos_scheduler->logger, "espacio libre enviado");
        break;
      }
      case OP_PEDIR_TAMANIO_PROCESO:
      {
        int a;
        uint32_t* pid =
            (uint32_t*)recibir_buffer(&a, datos_scheduler->socket_scheduler);
        t_proceso* proceso = buscar_proceso(
            datos_scheduler->procesos, datos_scheduler->mutex_procesos, *pid);
        int tamanio = calcular_tamanio_proceso(proceso);
        logger_info(datos_scheduler->logger,
                    "tamanio de proceso requerido es de: %d", tamanio);
        enviar_buffer(OP_TAMANIO_PROCESO, &tamanio, sizeof(int),
                      datos_scheduler->socket_scheduler);
        free(pid);
        break;
      }
      case OP_SUSPENDER_PROCESO:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una solicitud de SUSPENDER_PROCESO");
        int a;
        uint32_t* pid =
            (uint32_t*)recibir_buffer(&a, datos_scheduler->socket_scheduler);
        t_proceso* proceso_a_suspender = buscar_proceso(
            datos_scheduler->procesos, datos_scheduler->mutex_procesos, *pid);
        logger_info(datos_scheduler->logger, "PID recibido: %u", *pid);

        suspender_proceso(proceso_a_suspender, datos_scheduler);
        free(pid);
        break;
      }
      case OP_DES_SUSPENDER_PROCESO:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una solicitud de DES_SUSPENDER_PROCESO");
        int a;
        uint32_t* pid =
            (uint32_t*)recibir_buffer(&a, datos_scheduler->socket_scheduler);
        des_suspender_proceso(*pid, datos_scheduler);
        free(pid);
        break;
      }
      case OP_CIERRE_KERNEL_SCHEDULER:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una solicitud de cerrar comunicaciones");
        conexion_estable = false;
        break;
      }
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;
      default:
        logger_error(datos_scheduler->logger,
                     "Error codigo de operacion no reconocido");
        conexion_estable = false;
        break;
    }
  }
  logger_info(datos_scheduler->logger, "Cierre de escucha del scheduler");
  enviar_string(OP_MEMORIA_CORRUPTA, "Cierre de kernel",
                datos_scheduler->socket_scheduler);
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

        t_proceso* proceso =
            buscar_proceso(datos_cpu->procesos, datos_cpu->mutex_procesos, pid);
        char* instruccion = proceso->instrucciones[pc];
        logger_info(datos_cpu->logger,
                    "## PID: %u - Obtener instrucción: %u - Instrucción: %s",
                    pid, pc, instruccion);
        usleep(datos_cpu->instruction_delay * 1000);
        enviar_string(OP_ENVIAR_INSTRUCCION, instruccion,
                      datos_cpu->socket_cpu);
        list_destroy_and_destroy_elements(paquete, free);

        break;
      }
      case OP_PEDIR_CONTEXTO:
      {
        int a;
        uint32_t* pid = (uint32_t*)recibir_buffer(&a, datos_cpu->socket_cpu);
        t_proceso* proceso = buscar_proceso(datos_cpu->procesos,
                                            datos_cpu->mutex_procesos, *pid);
        if (proceso == NULL)
        {
          logger_error(datos_cpu->logger, "Proceso con pid %d no encontrado",
                       *pid);
          free(pid);
          break;
        }
        logger_info(datos_cpu->logger, "PID: %d - Obtener registro", *pid);
        logger_info(datos_cpu->logger, "delay de la instruccion %d",
                    datos_cpu->instruction_delay);
        usleep(datos_cpu->instruction_delay * 1000);
        enviar_buffer(OP_ENVIAR_CONTEXTO, &proceso->registro,
                      sizeof(t_registros), datos_cpu->socket_cpu);
        logger_info(datos_cpu->logger, "Enviando tabla de segmentos");
        t_paquete* tabla_segmentos_proceso =
            crear_paquete(OP_TABLA_DE_SEGMENTOS);
        t_list* lista_segmentos = filtrar_segmentos_proceso(
            *pid, datos_cpu->memoria_principal, datos_cpu->logger);

        agregar_segmentos_a_paquete(lista_segmentos, tabla_segmentos_proceso);

        list_destroy(lista_segmentos);
        enviar_paquete(tabla_segmentos_proceso, datos_cpu->socket_cpu);
        logger_info(datos_cpu->logger, "Tabla de segmentos enviada");
        eliminar_paquete(tabla_segmentos_proceso);
        free(pid);
        break;
      }
      case OP_CONTEXTO_ACTUALIZADO:
      {
        t_list* paquete = recibir_paquete(datos_cpu->socket_cpu);
        uint32_t pid = *(uint32_t*)list_get(paquete, 0);
        t_registros registros = *(t_registros*)list_get(paquete, 1);
        t_proceso* proceso =
            buscar_proceso(datos_cpu->procesos, datos_cpu->mutex_procesos, pid);
        if (proceso != NULL)
        {
          pthread_mutex_lock(datos_cpu->mutex_procesos);
          proceso->registro = registros;
          pthread_mutex_unlock(datos_cpu->mutex_procesos);
        }
        list_destroy_and_destroy_elements(paquete, free);
        break;
      }
      case OP_TABLA_SEG_ACTUALIZADA:
      {
        logger_info(datos_cpu->logger,
                    "CPU requiere actualizar la tabla de segmentos");
        int a;
        uint32_t* pid = (uint32_t*)recibir_buffer(&a, datos_cpu->socket_cpu);
        t_proceso* proceso = buscar_proceso(datos_cpu->procesos,
                                            datos_cpu->mutex_procesos, *pid);
        if (proceso == NULL)
        {
          logger_info(datos_cpu->logger,
                      "No se encontró el proceso con PID %u para terminar",
                      *pid);
          free(pid);
          break;
        }
        logger_info(datos_cpu->logger, "Enviando tabla de segmentos a cpu : %d",
                    datos_cpu->id);
        t_paquete* tabla_segmentos_proceso =
            crear_paquete(OP_TABLA_DE_SEGMENTOS);
        t_list* lista_segmentos = filtrar_segmentos_proceso(
            *pid, datos_cpu->memoria_principal, datos_cpu->logger);

        agregar_segmentos_a_paquete(lista_segmentos, tabla_segmentos_proceso);

        list_destroy(lista_segmentos);
        enviar_paquete(tabla_segmentos_proceso, datos_cpu->socket_cpu);
        logger_info(datos_cpu->logger, "Tabla de segmentos enviada a cpu");
        eliminar_paquete(tabla_segmentos_proceso);
        free(pid);
        break;
      }
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;
      default:
        conexion_estable = false;
        break;
    }
  }
  cerrar_cpu(datos_cpu);
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
