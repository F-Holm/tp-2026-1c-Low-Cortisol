#ifndef UTILS_SERVER_H_
#define UTILS_SERVER_H_

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int iniciar_servidor(char* puerto);
int esperar_cliente(int);

#endif /* UTILS_SERVER_H_ */
