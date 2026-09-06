#ifndef CPU_CONEXIONES_H_
#define CPU_CONEXIONES_H_

#include <stdio.h>

#include "cpu/cpu.h"
#include "utils/client.h"
#include "utils/config.h"
#include "utils/msg.h"
#include "utils/registros.h"

bool iniciar_conexion_kmemory(t_cpu* cpu);
bool iniciar_conexion_scheduler(t_cpu* cpu);
bool conectar_memory_stick(t_cpu* cpu);
uint32_t calcular_offset(t_list* sticks);
bool handshake_memory_stick(t_cpu* cpu, int nuevo_socket);
void avisar_bsod(t_cpu* cpu);

#endif /* CPU_CONEXIONES_H_ */