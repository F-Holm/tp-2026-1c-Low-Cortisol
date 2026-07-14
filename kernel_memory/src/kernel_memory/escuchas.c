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
          free(syscall);
        }
        else
        {
          crear_segmento(syscall->id_segmento, syscall->pid, syscall->tamanio,
                         datos_scheduler->memoria_principal,
                         datos_scheduler->socket_scheduler,
                         datos_scheduler->logger);
        }

        break;
      }
      case OP_SYSCALL_MEM_FREE:
      {
        logger_info(datos_scheduler->logger, "Llego una syscall de MEM_FREE");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)recibir_buffer(
            &a, datos_scheduler->socket_scheduler);
        eliminar_segmento(syscall->pid, syscall->id_segmento,
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
          break;
        }
        int offset_en_stick = 0;
        int indice = encontrar_stick(
            dir_fisica, datos_scheduler->sticks_conectados,
            datos_scheduler->mutex_lista_sockets, &offset_en_stick);
        pthread_mutex_lock(datos_scheduler->mutex_lista_sockets);
        t_datos_stick* stick_a_escribir_inicial =
            list_get(datos_scheduler->sticks_conectados, indice);
        int tamanio = stick_a_escribir_inicial->tamanio_stick - dir_fisica -
                      peticion_stdin->tamanio_a_leer;
        t_paquete* paquete = crear_paquete(OP_MEMORY_STICK_ESCRIBIR);
        if (tamanio < 0)
        {
          int tamanio_sumado =
              stick_a_escribir_inicial->tamanio_stick - dir_fisica;
          char* cadena_cortada = cortar_cadena(tamanio_sumado, string_escribir);
          int tamanio_cortado = strlen(cadena_cortada);
          agregar_a_paquete(paquete, &dir_fisica, sizeof(int));
          agregar_string_a_paquete(paquete, cadena_cortada);
          agregar_a_paquete(paquete, &tamanio_cortado, sizeof(int));
          enviar_paquete(paquete, stick_a_escribir_inicial->socket_stick);
          eliminar_paquete(paquete);
          char* mensaje = recibir_string(OP_MEMORY_STICK_ESCRITO);
          free(mensaje);
          logger_info(datos_scheduler->logger,
                      "##PID: %d - Escritura - Dir. Fisica: %d - Tamaño: %d",
                      peticion_stdin->pid, dir_fisica, tamanio_cortado);

          for (int i = indice + 1;
               tamanio_sumado < peticion_stdin->tamanio_a_leer; i++)
          {
            t_datos_stick* stick_a_escribir =
                list_get(datos_scheduler->sticks_conectados, i);
            tamanio_sumado = +stick_a_escribir->tamanio_stick;
            t_paquete* paquete2 = crear_paquete(OP_MEMORY_STICK_ESCRIBIR);
            agregar_a_paquete(paquete2, 0, sizeof(int));
            char* cadena_cortada =
                cortar_cadena(stick_a_escribir->tamanio_stick,
                              string_escribir + tamanio_sumado);
            int tamanio_cortado2 = strlen(cadena_cortada);
            agregar_string_a_paquete(paquete2, cadena_cortada);
            agregar_a_paquete(paquete2, &tamanio_cortado2, sizeof(int));
            enviar_paquete(paquete2, stick_a_escribir->socket_stick);
            free(cadena_cortada);
            eliminar_paquete(paquete2);
            char* mensaje = recibir_string(OP_MEMORY_STICK_ESCRITO);
            free(mensaje);
            logger_info(datos_scheduler->logger,
                        "##PID: %d - Escritura - Dir. Fisica: %d - Tamaño: %d",
                        peticion_stdin->pid, 0, tamanio_cortado2);
          }
          free(cadena_cortada);
        }
        else
        {
          int tamanio_string = strlen(string_escribir);
          agregar_a_paquete(paquete, &dir_fisica, sizeof(int));
          agregar_string_a_paquete(paquete, string_escribir);
          agregar_a_paquete(paquete, &tamanio_string, sizeof(int));
          enviar_paquete(paquete, stick_a_escribir_inicial->socket_stick);
          eliminar_paquete(paquete);
          char* mensaje = recibir_string(OP_MEMORY_STICK_ESCRITO);
          free(mensaje);
          logger_info(datos_scheduler->logger,
                      "##PID: %d - Escritura - Dir. Fisica: %d - Tamaño: %d",
                      peticion_stdin->pid, dir_fisica, tamanio_string);
        }
        pthread_mutex_unlock(datos_scheduler->mutex_lista_sockets);
        enviar_string(OP_OK, "OK", datos_scheduler->socket_scheduler);
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

        if (dir_fisica == -1)
        {
          enviar_string(OP_RESPUESTA_STDOUT, "Segmentation Fault",
                        datos_scheduler->socket_scheduler);
          break;
        }
        char* buffer = leer_de_sticks(
            dir_fisica, peticion_stdout->tamanio_a_escribir,
            datos_scheduler->sticks_conectados,
            datos_scheduler->mutex_lista_sockets, datos_scheduler->logger);
        if (buffer == NULL)
        {
          enviar_string(OP_RESPUESTA_STDOUT, "Error lectura stick",
                        datos_scheduler->socket_scheduler);
          break;
        }
        enviar_string(OP_RESPUESTA_STDOUT, buffer,
                      datos_scheduler->socket_scheduler);
        free(buffer);
        free(peticion_stdout);
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
        break;
      }
      case OP_SUSPENDER_PROCESO:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una solicitud de SUSPENDER_PROCESO");
        int a;
        uint32_t* pid = (uint32_t*)recibir_buffer(&a, datos_scheduler->socket_scheduler);
        t_proceso* proceso_a_suspender = buscar_proceso(datos_scheduler->procesos, datos_scheduler->mutex_procesos, *pid); // SE USA ESTA LISTA DE PROCESOS? 
        suspender_proceso(proceso_a_suspender, datos_scheduler);
        free(pid);
        break;
      }
      case OP_DES_SUSPENDER_PROCESO:
      {
        logger_info(datos_scheduler->logger,
                    "Llego una solicitud de DES_SUSPENDER_PROCESO");
        int a;
        uint32_t* pid = (uint32_t*)recibir_buffer(&a, datos_scheduler->socket_scheduler);
        des_suspender_proceso(*pid, datos_scheduler);
        free(pid);
        break;
      }
      case OP_CIERRE_KERNEL_SCHEDULER:
      {
        liberar_datos_scheduler(datos_scheduler);
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

        t_proceso* proceso =
            buscar_proceso(datos_cpu->procesos, datos_cpu->mutex_procesos, pid);
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
        t_proceso* proceso = buscar_proceso(datos_cpu->procesos,
                                            datos_cpu->mutex_procesos, *pid);
        if (proceso == NULL)
        {
          logger_error(datos_cpu->logger, "PROCESO NO ENCONTRADO");
          break;
        }
        logger_info(datos_cpu->logger, "## PID: %d - Obtener registro", *pid);
        logger_info(datos_cpu->logger, "delay de la instruccion %d",
                    datos_cpu->instruction_delay);
        usleep(datos_cpu->instruction_delay * 1000);
        enviar_buffer(OP_ENVIAR_CONTEXTO, &proceso->registro,
                      sizeof(t_registros), datos_cpu->socket_cpu);
        logger_info(datos_cpu->logger, "Enviando tabla de segmentos");
        t_paquete* tabla_segmentos_proceso =
            crear_paquete(OP_TABLA_DE_SEGMENTOS);
        agregar_segmentos_a_paquete(
            filtrar_segmentos_proceso(*pid, datos_cpu->memoria_principal,
                                      datos_cpu->logger),
            tabla_segmentos_proceso);
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
          logger_info(datos_cpu->logger, "PROCESO NO ENCONTRADO");
          break;
        }
        logger_info(datos_cpu->logger, "Enviando tabla de segmentos a cpu : %d",
                    datos_cpu->id);
        t_paquete* tabla_segmentos_proceso =
            crear_paquete(OP_TABLA_DE_SEGMENTOS);
        agregar_segmentos_a_paquete(
            filtrar_segmentos_proceso(*pid, datos_cpu->memoria_principal,
                                      datos_cpu->logger),
            tabla_segmentos_proceso);
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
