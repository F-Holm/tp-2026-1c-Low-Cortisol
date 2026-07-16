#ifndef CPU_MEMORIA_H_
#define CPU_MEMORIA_H_

#include <stdio.h>

#include "cpu/cpu.h"

#define DIR_INVALIDA UINT32_MAX

uint32_t mmu(t_cpu* cpu, t_contexto* contexto, uint32_t dir_logica,
             uint32_t tamanio, uint32_t pid);
t_segmento* buscar_segmento_por_id(t_list* tablaSegmentos,
                                   uint32_t num_segmento);
bool seg_fault_KS(t_cpu* cpu, uint32_t pid);
t_memory_stick_info* encontrar_stick(t_cpu* cpu, uint32_t dir_fisica);
void* leer_memoria(t_cpu* cpu, uint32_t dir_fisica, uint32_t tamanio);
bool solicitar_lectura_MS(t_cpu* cpu, t_memory_stick_info* stick,
                          uint32_t dir_en_stick, uint32_t bytes_a_leer);
char* confirmacion_letura_MS(t_cpu* cpu, t_memory_stick_info* stick);
bool escribir_memoria(t_cpu* cpu, uint32_t dir_fisica, void* datos_a_escribir,
                      uint32_t tamanio);
bool solicitar_escritura_MS(t_cpu* cpu, t_memory_stick_info* stick,
                            uint32_t dir_en_stick, void* datos,
                            uint32_t bytes_a_escribir);
bool confirmacion_escritura_MS(t_cpu* cpu, t_memory_stick_info* stick);

#endif /* CPU_MEMORIA_H_ */