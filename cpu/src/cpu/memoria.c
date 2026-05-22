#include "cpu/cpu.h"
#include "cpu/memoria.h"

#include <stdio.h>

uint32_t mmu(t_cpu* cpu, t_contexto* contexto, uint32_t dir_logica, uint32_t tamaño)
{
    log_info(cpu->logger, "funcion mmu llamada");
    return 0;
}