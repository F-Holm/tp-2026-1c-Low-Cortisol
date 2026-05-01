#ifndef KERNEL_MEMORY_PROTOCOLO_H_
#define KERNEL_MEMORY_PROTOCOLO_H_

#include "kernel_memory/estructuras.h"

bool recibir_id_cpu(t_datos_cpu* datos_cpu);
bool recibir_tamanio_stick(t_datos_stick* datos_stick);
bool recibir_puerto_escucha_stick(t_datos_stick* datos_stick);
void agregar_conexion_stick(t_datos_kernel_mem* datos_kernel_memory,
                            t_datos_stick* datos_stick);
void enviar_sticks_conectadas(t_datos_kernel_mem* datos_kernel_memory,
                              t_datos_cpu* datos_cpu);
void enviar_conexion_cpu(t_datos_stick* datos_stick, t_list* cpus_conectados);
void agregar_conexion_cpu(t_datos_kernel_mem* datos_kernel_memory,
                          t_datos_cpu* datos_cpu);

#endif