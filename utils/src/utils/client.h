#ifndef UTILS_CLIENT_H_
#define UTILS_CLIENT_H_

#include <commons/log.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int crear_conexion(char* ip, char* puerto);
void liberar_conexion(int socket_fd);

#endif /* UTILS_CLIENT_H_ */
