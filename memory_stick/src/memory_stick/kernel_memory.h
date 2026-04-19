#ifndef MEMORY_STICK_KERNEL_MEMORY_H_
#define MEMORY_STICK_KERNEL_MEMORY_H_

#include <commons/log.h>
#include <stdbool.h>
#include <stdint.h>

int conectar_km(char* ip, char* puerto, t_log* logger);
bool enviar_puerto_server_ms_km(int socket, uint16_t puerto);

#endif /* MEMORY_STICK_KERNEL_MEMORY_H_ */
