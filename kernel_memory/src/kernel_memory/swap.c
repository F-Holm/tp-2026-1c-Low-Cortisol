#include "kernel_memory/swap.h"

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
}

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

static void agregar_bloque_a_lista_swap(t_segmento* segmento, int contador, t_datos_swap* datos_swap, t_logger* logger)
{
    t_datos_bloque* bloque = malloc(sizeof(t_datos_bloque));
    bloque->num_segmento = segmento->num_segmento;
    bloque->num_bloque_del_segmento = contador;
    bloque->pid = segmento->pid;
    bloque->num_bloque = seleccionar_bloque_libre(datos_swap);
    list_replace(datos_swap->lista_bloques, bloque->num_bloque, bloque);
    logger_info(logger, "Agregando bloque a swap: PID %d, Segmento %d, Bloque del segmento %d.", bloque->pid, bloque->num_segmento, bloque->num_bloque_del_segmento);
}

void escribir_bloque_en_swap(int num_bloque, char* contenido, t_datos_swap* swap) 
{
    t_paquete* paquete = crear_paquete(OP_ESCRIBIR_DISCO);
    agregar_a_paquete(paquete, &num_bloque, sizeof(int)); 
    strcat(contenido, "\0");
    agregar_a_paquete(paquete, contenido, strlen(contenido) + 1);
    enviar_paquete(paquete, swap->socket_swap);
    eliminar_paquete(paquete);
}

void suspender_proceso(t_proceso* proceso_a_suspender, t_datos_scheduler* datos_scheduler)
{
    int contador = 0;
    //t_list* lista_auxiliar = filtrar_segmentos_proceso(proceso_a_suspender->pid, datos_scheduler->memoria_principal, datos_scheduler->logger);
    t_list_iterator* iterador = list_iterator_create(datos_scheduler->memoria_principal->segmentos);
    while (list_iterator_has_next(iterador)) {
        t_datos_bloque* segmento_actual = list_iterator_next(iterador);
        if (segmento_actual->pid == proceso_a_suspender->pid) {
            escribir_bloque_en_swap(segmento_actual->num_bloque, /*definir de donde obtengo el contenido*/, datos_scheduler->datos_swap);
            agregar_bloque_a_lista_swap(segmento_actual, contador, datos_scheduler->datos_swap, datos_scheduler->logger);
            // llamar a eliminar_segmento y eliminar el recien escrito 
            eliminar_segmento(segmento_actual->num_segmento, proceso_a_suspender->pid, datos_scheduler->memoria_principal, datos_scheduler->logger);
            contador++;
        }
    }
    list_iterator_destroy(iterador);
}
