#include "kernel_memory/escuchas.h"

void* escucha_scheduler(void* ptr)
{
  t_datos_scheduler* datos_scheduler = (t_datos_scheduler*)ptr;
  bool conexion_estable = true;
  while (conexion_estable)
  {
    switch (receive_op_code(datos_scheduler->socket_scheduler))
    {
      case OP_NEW_PROCESS:
      {
        t_list* packet = receive_packet(datos_scheduler->socket_scheduler);
        char* path_relativo = list_get(packet, 0);
        u_int32_t* pid = list_get(packet, 1);

        t_proceso* proceso = inicializar_proceso(
            *pid, path_relativo, datos_scheduler->scripts_basepath,
            datos_scheduler->logger);

        aniadir_lista_mtx(datos_scheduler->procesos,
                          datos_scheduler->mutex_procesos, proceso);

        log_info(datos_scheduler->logger, "## PID: %d  - Proceso Creado", *pid);
        list_destroy_and_destroy_elements(packet, free);
        send_string(OP_PROCESS_STARTED, "Proceso creado",
                    datos_scheduler->socket_scheduler);
        break;
      }
      case OP_SYSCALL_MEM_ALLOC:
      {
        log_info(datos_scheduler->logger, "Llego una syscall de MEM_ALLOC");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)receive_buffer(
            &a, datos_scheduler->socket_scheduler);

        if (syscall->size >
            datos_scheduler->memoria_principal->tamanio_maximo_segmento)
        {
          log_info(
              datos_scheduler->logger,
              "La syscall de MEM_ALLOC no se pudo realizar ya que el tamaño "
              "solicitado es mayor al tamaño máximo de segmento");
          send_string(OP_SEGMENT_SIZE_EXCEEDED,
                      "Tamaño solicitado es mayor al tamaño máximo de segmento",
                      datos_scheduler->socket_scheduler);
        }
        else
        {
          crear_segmento(syscall->segment_id, syscall->pid, syscall->size,
                         datos_scheduler->memoria_principal,
                         datos_scheduler->socket_scheduler,
                         datos_scheduler->logger);
        }
        free(syscall);
        break;
      }
      case OP_SYSCALL_MEM_FREE:
      {
        log_info(datos_scheduler->logger, "Llego una syscall de MEM_FREE");
        int a;
        t_syscall_memory* syscall = (t_syscall_memory*)receive_buffer(
            &a, datos_scheduler->socket_scheduler);
        eliminar_segmento(syscall->segment_id, syscall->pid,
                          datos_scheduler->memoria_principal,
                          datos_scheduler->logger);
        log_info(datos_scheduler->logger, "se ha eliminado correctamente");
        free(syscall);
        send_string(OP_MEMORY_FREED, "Memoria liberada",
                    datos_scheduler->socket_scheduler);
        break;
      }
      case OP_IO_STDIN_REQUEST:
      {
        log_info(datos_scheduler->logger,
                 "Llego una syscall de PETICION_IO_STDIN");
        t_list* paquete_stdin =
            receive_packet(datos_scheduler->socket_scheduler);
        t_stdin_request* peticion_stdin =
            (t_stdin_request*)list_get(paquete_stdin, 0);
        char* buffer_escribir = (char*)list_get(paquete_stdin, 1);
        int dir_fisica = traducir_direccion_logica(
            peticion_stdin->pid, peticion_stdin->logical_address,
            peticion_stdin->bytes_to_read, datos_scheduler->memoria_principal,
            datos_scheduler->logger);
        if (dir_fisica == -1)
        {
          send_string(OP_STDIN_RESPONSE, "Segmentation Fault",
                      datos_scheduler->socket_scheduler);
          list_destroy_and_destroy_elements(paquete_stdin, free);
          break;
        }
        log_info(datos_scheduler->logger,
                 "## PID: %u - Escritura - "
                 "Dir. Fisica: %u - Tamaño: %d",
                 peticion_stdin->pid, dir_fisica,
                 peticion_stdin->bytes_to_read);

        int tamanio_pedido = peticion_stdin->bytes_to_read;
        char* buffer_seguro = calloc(tamanio_pedido, sizeof(char));

        size_t bytes_validos = strnlen(buffer_escribir, tamanio_pedido);
        memcpy(buffer_seguro, buffer_escribir, bytes_validos);

        if (!escribir_en_sticks(
                peticion_stdin->pid, dir_fisica, tamanio_pedido, buffer_seguro,
                datos_scheduler->sticks_conectados,
                datos_scheduler->mutex_lista_sockets, datos_scheduler->logger,
                datos_scheduler->socket_scheduler))
        {
          log_error(datos_scheduler->logger, "Error al escribir en sticks");
          conexion_estable = false;
          free(buffer_seguro);
          list_destroy_and_destroy_elements(paquete_stdin, free);
          break;
        }
        free(buffer_seguro);
        send_string(OP_STDIN_RESPONSE, "Memoria Escrita",
                    datos_scheduler->socket_scheduler);
        log_info(datos_scheduler->logger, "Peticion STDIN finalizada");
        list_destroy_and_destroy_elements(paquete_stdin, free);
        break;
      }
      case OP_IO_STDOUT_REQUEST:
      {
        log_info(datos_scheduler->logger,
                 "Llego una syscall de PETICION_IO_STDOUT");
        int size;
        t_stdout_request* peticion_stdout = (t_stdout_request*)receive_buffer(
            &size, datos_scheduler->socket_scheduler);
        int dir_fisica = traducir_direccion_logica(
            peticion_stdout->pid, peticion_stdout->logical_address,
            peticion_stdout->bytes_to_write, datos_scheduler->memoria_principal,
            datos_scheduler->logger);
        log_info(datos_scheduler->logger,
                 "## PID: %u - Lectura - "
                 "Dir. Fisica: %u - Tamaño: %d",
                 peticion_stdout->pid, dir_fisica,
                 peticion_stdout->bytes_to_write);
        if (dir_fisica == -1)
        {
          send_string(OP_STDOUT_RESPONSE, "Segmentation Fault",
                      datos_scheduler->socket_scheduler);
          free(peticion_stdout);
          break;
        }
        char* buffer = leer_de_sticks(
            dir_fisica, peticion_stdout->bytes_to_write,
            datos_scheduler->sticks_conectados,
            datos_scheduler->mutex_lista_sockets, datos_scheduler->logger,
            datos_scheduler->socket_scheduler);
        if (buffer == NULL)
        {
          send_string(OP_STDOUT_RESPONSE, "Error lectura stick",
                      datos_scheduler->socket_scheduler);
          conexion_estable = false;
          free(peticion_stdout);
          break;
        }
        send_string(OP_STDOUT_RESPONSE, buffer,
                    datos_scheduler->socket_scheduler);
        free(buffer);
        free(peticion_stdout);
        log_info(datos_scheduler->logger, "Peticion STDOUT finalizada");
        break;
      }
      case OP_END_PROCESS:
      {
        log_info(datos_scheduler->logger,
                 "Llego una solicitud de TERMINAR_PROCESO");
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, datos_scheduler->socket_scheduler);
        t_proceso* proceso_a_terminar = buscar_proceso(
            datos_scheduler->procesos, datos_scheduler->mutex_procesos, *pid);
        t_list* lista_segmentos = filtrar_segmentos_proceso(
            *pid, datos_scheduler->memoria_principal, datos_scheduler->logger);
        if (proceso_a_terminar != NULL)
        {
          pthread_mutex_lock(datos_scheduler->mutex_procesos);
          list_remove_element(datos_scheduler->procesos, proceso_a_terminar);
          pthread_mutex_unlock(datos_scheduler->mutex_procesos);
          log_info(datos_scheduler->logger, "Proceso con PID %u terminado",
                   *pid);
          t_list_iterator* iterador_segmentos =
              list_iterator_create(lista_segmentos);
          while (list_iterator_has_next(iterador_segmentos))
          {
            t_segment* segmento = list_iterator_next(iterador_segmentos);
            eliminar_segmento(segmento->id, proceso_a_terminar->pid,
                              datos_scheduler->memoria_principal,
                              datos_scheduler->logger);
          }
          list_iterator_destroy(iterador_segmentos);
          liberar_proceso(proceso_a_terminar);
        }
        else
        {
          log_info(datos_scheduler->logger,
                   "No se encontró el proceso con PID %u para terminar", *pid);
        }
        list_destroy(lista_segmentos);
        free(pid);
        break;
      }
      case OP_REQUEST_FREE_MEMORY:
      {
        free(receive_string(datos_scheduler->socket_scheduler));
        log_info(datos_scheduler->logger,
                 "Se requiere la memoria disponible por parte del scheduler");

        int tamanio = calcular_espacio_libre(
            datos_scheduler->memoria_principal->huecos,
            datos_scheduler->memoria_principal->mutex_memoria_principal,
            datos_scheduler->logger);
        log_info(datos_scheduler->logger, "espacio libre calculado");
        send_buffer(OP_FREE_MEMORY, &tamanio, sizeof(int),
                    datos_scheduler->socket_scheduler);
        log_info(datos_scheduler->logger, "espacio libre enviado");
        break;
      }
      case OP_REQUEST_PROCESS_SIZE:
      {
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, datos_scheduler->socket_scheduler);
        t_proceso* proceso = buscar_proceso(
            datos_scheduler->procesos, datos_scheduler->mutex_procesos, *pid);
        int tamanio = calcular_tamanio_proceso(
            proceso, datos_scheduler->memoria_principal);
        send_buffer(OP_PROCESS_SIZE, &tamanio, sizeof(int),
                    datos_scheduler->socket_scheduler);
        free(pid);
        break;
      }
      case OP_SUSPEND_PROCESS:
      {
        log_info(datos_scheduler->logger,
                 "Llego una solicitud de SUSPENDER_PROCESO");
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, datos_scheduler->socket_scheduler);
        t_proceso* proceso_a_suspender = buscar_proceso(
            datos_scheduler->procesos, datos_scheduler->mutex_procesos, *pid);
        log_info(datos_scheduler->logger, "PID recibido: %u", *pid);

        suspender_proceso(proceso_a_suspender, datos_scheduler);
        free(pid);
        break;
      }
      case OP_RESUME_SUSPENDED_PROCESS:
      {
        log_info(datos_scheduler->logger,
                 "Llego una solicitud de DES_SUSPENDER_PROCESO");
        int a;
        uint32_t* pid =
            (uint32_t*)receive_buffer(&a, datos_scheduler->socket_scheduler);
        des_suspender_proceso(*pid, datos_scheduler);
        free(pid);
        break;
      }
      case OP_KERNEL_SCHEDULER_SHUTDOWN:
      {
        log_info(datos_scheduler->logger,
                 "Llego una solicitud de cerrar comunicaciones");
        conexion_estable = false;
        break;
      }
      case OP_KERNEL_MEMORY_RUNNING:
        free(receive_string(datos_scheduler->socket_scheduler));
        break;
      case OP_CODE_ERROR:
        conexion_estable = false;
        break;
      default:
        log_error(datos_scheduler->logger,
                  "Error codigo de operacion no reconocido");
        conexion_estable = false;
        break;
    }
  }
  log_info(datos_scheduler->logger, "Cierre de escucha del scheduler");
  send_string(OP_MEMORY_CORRUPTED, "Cierre de kernel",
              datos_scheduler->socket_scheduler);
  pthread_mutex_lock(datos_scheduler->mutex_hilos_activos);
  (*datos_scheduler->hilos_activos)--;
  pthread_cond_signal(datos_scheduler->cond_hilos_activos);
  pthread_mutex_unlock(datos_scheduler->mutex_hilos_activos);
  liberar_datos_scheduler(datos_scheduler);
  return NULL;
}

void* escucha_cpu(void* ptr)
{
  t_datos_cpu* datos_cpu = (t_datos_cpu*)ptr;
  bool conexion_estable = true;
  while (conexion_estable)
  {
    switch (receive_op_code(datos_cpu->socket_cpu))
    {
      case OP_NEXT_INSTRUCTION:
      {
        t_list* packet = receive_packet(datos_cpu->socket_cpu);
        uint32_t pid = *(uint32_t*)list_get(packet, 0);
        uint32_t pc = *(uint32_t*)list_get(packet, 1);

        t_proceso* proceso =
            buscar_proceso(datos_cpu->procesos, datos_cpu->mutex_procesos, pid);
        char* instruccion = proceso->instrucciones[pc];
        log_info(datos_cpu->logger,
                 "## PID: %u - Obtener instrucción: %u - Instrucción: %s", pid,
                 pc, instruccion);
        usleep(datos_cpu->instruction_delay * 1000);
        send_string(OP_SEND_INSTRUCTION, instruccion, datos_cpu->socket_cpu);
        list_destroy_and_destroy_elements(packet, free);

        break;
      }
      case OP_REQUEST_CONTEXT:
      {
        int a;
        uint32_t* pid = (uint32_t*)receive_buffer(&a, datos_cpu->socket_cpu);
        t_proceso* proceso = buscar_proceso(datos_cpu->procesos,
                                            datos_cpu->mutex_procesos, *pid);
        if (proceso == NULL)
        {
          log_error(datos_cpu->logger, "Proceso con pid %d no encontrado",
                    *pid);
          free(pid);
          break;
        }
        log_info(datos_cpu->logger, "PID: %d - Obtener registro", *pid);
        log_info(datos_cpu->logger, "delay de la instruccion %d",
                 datos_cpu->instruction_delay);
        usleep(datos_cpu->instruction_delay * 1000);
        send_buffer(OP_SEND_CONTEXT, &proceso->registro, sizeof(t_registers),
                    datos_cpu->socket_cpu);
        log_info(datos_cpu->logger, "Enviando tabla de segmentos");
        t_packet* tabla_segmentos_proceso = create_packet(OP_SEGMENT_TABLE);
        t_list* lista_segmentos = filtrar_segmentos_proceso(
            *pid, datos_cpu->memoria_principal, datos_cpu->logger);

        agregar_segmentos_a_paquete(lista_segmentos, tabla_segmentos_proceso);

        list_destroy(lista_segmentos);
        send_packet(tabla_segmentos_proceso, datos_cpu->socket_cpu);
        log_info(datos_cpu->logger, "Tabla de segmentos enviada");
        destroy_packet(tabla_segmentos_proceso);
        free(pid);
        break;
      }
      case OP_UPDATED_CONTEXT:
      {
        t_list* packet = receive_packet(datos_cpu->socket_cpu);
        uint32_t pid = *(uint32_t*)list_get(packet, 0);
        t_registers registros = *(t_registers*)list_get(packet, 1);
        t_proceso* proceso =
            buscar_proceso(datos_cpu->procesos, datos_cpu->mutex_procesos, pid);
        if (proceso != NULL)
        {
          pthread_mutex_lock(datos_cpu->mutex_procesos);
          proceso->registro = registros;
          pthread_mutex_unlock(datos_cpu->mutex_procesos);
        }
        list_destroy_and_destroy_elements(packet, free);
        break;
      }
      case OP_UPDATED_SEGMENT_TABLE:
      {
        log_info(datos_cpu->logger,
                 "CPU requiere actualizar la tabla de segmentos");
        int a;
        uint32_t* pid = (uint32_t*)receive_buffer(&a, datos_cpu->socket_cpu);
        t_proceso* proceso = buscar_proceso(datos_cpu->procesos,
                                            datos_cpu->mutex_procesos, *pid);
        if (proceso == NULL)
        {
          log_info(datos_cpu->logger,
                   "No se encontró el proceso con PID %u para terminar", *pid);
          free(pid);
          break;
        }
        log_info(datos_cpu->logger, "Enviando tabla de segmentos a cpu : %d",
                 datos_cpu->id);
        t_packet* tabla_segmentos_proceso = create_packet(OP_SEGMENT_TABLE);
        t_list* lista_segmentos = filtrar_segmentos_proceso(
            *pid, datos_cpu->memoria_principal, datos_cpu->logger);

        agregar_segmentos_a_paquete(lista_segmentos, tabla_segmentos_proceso);

        list_destroy(lista_segmentos);
        send_packet(tabla_segmentos_proceso, datos_cpu->socket_cpu);
        log_info(datos_cpu->logger, "Tabla de segmentos enviada a cpu");
        destroy_packet(tabla_segmentos_proceso);
        free(pid);
        break;
      }
      case OP_STICK_DISCONNECTED:
        log_info(datos_cpu->logger,
                 "Avisando al Kernel Scheduler que la memoria está corrupta");
        if (!send_string(OP_MEMORY_CORRUPTED, "Memoria corrupta",
                         datos_cpu->socket_scheduler))
        {
          log_error(datos_cpu->logger,
                    "No se pudo enviar el BSOD al Kernel Scheduler");
        }
        shutdown(datos_cpu->socket_scheduler, SHUT_RDWR);
      case OP_CODE_ERROR:
      default:
        conexion_estable = false;
        break;
    }
  }
  cerrar_cpu(datos_cpu);
  pthread_mutex_lock(datos_cpu->mutex_hilos_activos);
  (*datos_cpu->hilos_activos)--;
  pthread_cond_signal(datos_cpu->cond_hilos_activos);
  pthread_mutex_unlock(datos_cpu->mutex_hilos_activos);
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
