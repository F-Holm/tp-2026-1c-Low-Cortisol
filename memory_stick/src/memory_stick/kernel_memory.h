#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils/log.h"

bool handshake_km(int socket_km, t_log* logger);
bool enviar_tamanio(int socket_km, char* tamanio, t_log* logger);
int iniciar_conexion_km(char* ip, char* puerto, char* tamanio, t_log* logger);
int conectar_km(char* ip, char* puerto, t_log* logger);
bool enviar_puerto_server_ms_km(int socket, uint16_t puerto);
