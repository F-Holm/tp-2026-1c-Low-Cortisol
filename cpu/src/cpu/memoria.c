#include "cpu/memoria.h"
#include "cpu/registros.h"

#include <stdio.h>

#include "cpu/cpu.h"

uint32_t mmu(t_cpu* cpu, t_contexto* contexto, uint32_t dir_logica,
             uint32_t tamanio)
{
  uint32_t num_segmento   = dir_logica / cpu->tamanio_max_segmento;
  uint32_t desplazamiento = dir_logica % cpu->tamanio_max_segmento;
  
  

  if(desplazamiento + tamanio > segmento->limite)
  {
    
  }
  log_info(cpu->logger, "funcion mmu llamada");
  return 0;
}