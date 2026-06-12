#ifndef CPU_MEMORIA_H_
#define CPU_MEMORIA_H_

#include <stdio.h>

#include "cpu/cpu.h"

#define DIR_INVALIDA UINT32_MAX

uint32_t mmu(t_cpu* cpu, t_contexto* contexto, uint32_t dir_logica,
             uint32_t tamanio, uint32_t pid);
t_segmento* buscar_segmento_por_id(t_list* tablaSegmentos,
                                   uint32_t num_segmento);
void seg_fault_KS(t_cpu* cpu, uint32_t pid);

#endif /* CPU_MEMORIA_H_ */