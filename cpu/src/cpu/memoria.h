#ifndef CPU_MEMORIA_H_
#define CPU_MEMORIA_H_

#include <stdio.h>

#include "cpu/cpu.h"

uint32_t mmu(t_cpu* cpu, t_contexto* contexto, uint32_t dir_logica, uint32_t tamaño);

#endif /* CPU_MEMORIA_H_ */