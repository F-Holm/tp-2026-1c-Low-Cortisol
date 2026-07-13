#include "kernel_memory/swap.h"

/* FUNCION EN DESUSO POR AHORA
t_list* filtrar_bloques_por_pid(t_list* bloques, uint32_t pid) {
  t_list* resultado = list_create();
  t_list_iterator* iterador = list_iterator_create(bloques);
  while (list_iterator_has_next(iterador)) {
    t_datos_bloque* bloque = list_iterator_next(iterador);
    if (bloque->pid == pid)
      list_add(resultado, bloque);
  }
  list_iterator_destroy(iterador);
  return resultado;
}*/

static int seleccionar_bloque_libre(t_datos_swap* datos_swap)
{
    for(int i = 0; i < datos_swap->tamanio_swap/datos_swap->tamanio_bloque; i++)
    {
        t_datos_bloque* bloque = list_get(datos_swap->lista_bloques, i);
        if (bloque->pid == -1) return i;
    }
    logger_info(datos_swap->logger, "No hay bloques libres en swap");
    return -1;
}

static int agregar_bloque_a_lista_swap(t_segmento* segmento, int contador, t_datos_swap* datos_swap, t_logger* logger) 
/*retorna el numero de bloque agregado o -1 si no se pudo agregar*/
{
    int num_bloque_libre = seleccionar_bloque_libre(datos_swap);
    if(num_bloque_libre != -1)
    {
        t_datos_bloque* bloque_libre = list_get(datos_swap->lista_bloques, num_bloque_libre);
        bloque_libre->num_segmento = segmento->id;
        bloque_libre->num_bloque_del_segmento = contador;
        bloque_libre->pid = segmento->pid;
        logger_info(logger, "Agregando bloque a swap: PID %d, Segmento %d, Bloque del segmento %d.", bloque_libre->pid, bloque_libre->num_segmento, bloque_libre->num_bloque_del_segmento);
    } else 
    {
        logger_info(logger, "No se puede agregar el segmento a swap: No hay bloques libres.");
    }
    return num_bloque_libre;
}

static void escribir_bloque_en_swap(int num_bloque, char* contenido, t_datos_swap* swap) 
{
    t_paquete* paquete = crear_paquete(OP_ESCRIBIR_DISCO);
    agregar_a_paquete(paquete, &num_bloque, sizeof(int)); 
    char* buffer_auxiliar = calloc(swap->tamanio_bloque, 1);
    memcpy(buffer_auxiliar, contenido, /*FALTA CALCULAR EL TAMANIO DE CONTENIDO*/); 
    agregar_a_paquete(paquete, buffer_auxiliar, swap->tamanio_bloque);
    enviar_paquete(paquete, swap->socket_swap);
    eliminar_paquete(paquete);
    free(buffer_auxiliar);
}

void suspender_proceso(t_proceso* proceso_a_suspender, t_datos_scheduler* datos_scheduler)
{
    int tamanio_bloque = datos_scheduler->datos_swap->tamanio_bloque;
    bool proceso_suspendido = false;
    t_list_iterator* iterador = list_iterator_create(datos_scheduler->memoria_principal->segmentos);
    while (list_iterator_has_next(iterador)) 
    {
        t_segmento* segmento_actual = list_iterator_next(iterador);
        if (segmento_actual->pid == proceso_a_suspender->pid) 
        {
            int cant_bloques_x_segmento = (segmento_actual->size + tamanio_bloque - 1) / tamanio_bloque;
            bool segmento_suspendido = false;
            for (int i = 0; i < cant_bloques_x_segmento; i++)
            {
                int offset = i * tamanio_bloque;
                int bytes_a_leer = (segmento_actual->size - offset) < tamanio_bloque ? (segmento_actual->size - offset) : tamanio_bloque;
                // FALTA LOGICA PARA OBTENER EL CONTENIDO A LEER. VA ACA
                int num_bloque = agregar_bloque_a_lista_swap(segmento_actual, i, datos_scheduler->datos_swap, datos_scheduler->logger);
                if(num_bloque != -1)
                {
                    escribir_bloque_en_swap(num_bloque, /*CONTENIDO A LEER*/, datos_scheduler->datos_swap);
                    segmento_suspendido = true;
                } else 
                {
                    logger_info(datos_scheduler->logger, "No se pudo suspender el proceso PID %d: No hay bloques libres en swap.", proceso_a_suspender->pid);
                    enviar_string(OP_SUSPENSION_NO_EXITOSA, "No se pudo suspender el proceso porque swap esta lleno.", datos_scheduler->socket_scheduler);
                    segmento_suspendido = false;
                    break;
                }
            }
            if(segmento_suspendido)
            {
                eliminar_segmento(segmento_actual->id, proceso_a_suspender->pid, datos_scheduler->memoria_principal, datos_scheduler->logger); 
            } else 
            {
                list_iterator_destroy(iterador);
                return;
            }
            
        }
    }
    list_iterator_destroy(iterador);
    if(proceso_suspendido)
    {
        enviar_string(OP_SUSPENSION_EXITOSA, "", datos_scheduler->socket_scheduler);
    } else 
    {
        logger_info(datos_scheduler->logger, "No se encontro el proceso PID %d.", proceso_a_suspender->pid);
        enviar_string(OP_SUSPENSION_NO_EXITOSA, "No se pudo suspender el proceso porque no se encontro su pid.", datos_scheduler->socket_scheduler);
    }
}
