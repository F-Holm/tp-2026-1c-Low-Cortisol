#include "cpu/memoria.h"
#include "cpu/registros.h"

#include <stdio.h>

#include "cpu/cpu.h"

uint32_t mmu(t_cpu* cpu, t_contexto* contexto, uint32_t dir_logica,
             uint32_t tamanio, uint32_t pid)
{
  uint32_t num_segmento   = dir_logica / cpu->tamanio_max_segmento;
  uint32_t desplazamiento = dir_logica % cpu->tamanio_max_segmento;
  
  t_segmento* segmento = buscar_segmento_por_id(contexto->tablaDeSegmentos, num_segmento);
  
  if(segmento == NULL)
  {
    log_error(cpu->logger, "## SEGMENTO %u NO ENCONTRADO", num_segmento);
    cerrarModulo(cpu);
    return DIR_INVALIDA;
  }

  if(desplazamiento + tamanio > segmento->size)
  {
    seg_fault_KS(cpu, pid);
    return DIR_INVALIDA;
  }
  
  return segmento->base + desplazamiento;
}

t_segmento* buscar_segmento_por_id(t_list* tablaSegmentos, uint32_t num_segmento)
{
  t_segmento* segmento = NULL;
  for (int i = 0; i < list_size(tablaSegmentos); i++)
  {
    segmento = list_get(tablaSegmentos, i);
    if(segmento->id == num_segmento)
    {
      return segmento;
    }
  }
  return NULL;
}

void seg_fault_KS(t_cpu* cpu, uint32_t pid)
{
  if(!enviar_buffer(OP_SEG_FAULT , &pid, sizeof(uint32_t),
                      cpu->socket_kernel_scheduler))
  {
    log_error(cpu->logger, "## FALLO EN EL ENVIO DE SEGMENTATION FAULT");
    cerrar_modulo(cpu);
  }
  log_info(cpu->logger, "envio correcto seg fault");
}