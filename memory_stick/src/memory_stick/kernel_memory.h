#ifndef MEMORY_STICK_KERNEL_MEMORY_H_
#define MEMORY_STICK_KERNEL_MEMORY_H_

#include <commons/log.h>
#include <stdbool.h>
#include <stdint.h>

#include "utils/logger.h"

bool handshake_km(int socket_km, t_logger* logger);
bool enviar_tamanio(int socket_km, char* tamanio, t_logger* logger);
int iniciar_conexion_km(char* ip, char* puerto, char* tamanio, t_logger* logger);
int conectar_km(char* ip, char* puerto, t_logger* logger);
bool enviar_puerto_server_ms_km(int socket, uint16_t puerto);

#endif /* MEMORY_STICK_KERNEL_MEMORY_H_ */
