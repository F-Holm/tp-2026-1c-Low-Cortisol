#ifndef CPU_CONEXIONES_H_
#define CPU_CONEXIONES_H_

#include <commons/config.h>
#include <stdio.h>

#include "utils/client.h"
#include "utils/msg.h"
#include "utils/registros.h"

#include "cpu/cpu.h"

bool iniciar_conexion_kmemory(t_cpu* cpu);
bool iniciar_conexion_scheduler(t_cpu* cpu);
bool conectar_memory_stick(t_cpu* cpu);
bool handshake_memory_stick(t_cpu* cpu, int nuevo_socket);

#endif /* CPU_CONEXIONES_H_ */