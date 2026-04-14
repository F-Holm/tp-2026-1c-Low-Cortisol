#ifndef UTILS_SERVER_H_
#define UTILS_SERVER_H_

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <commons/collections/list.h>
#include <commons/log.h>

typedef enum
{
  MENSAJE,
  PAQUETE
} op_code;

extern t_log* logger;

void* recibir_buffer(int*, int);

int iniciar_servidor(void);
int esperar_cliente(int);
t_list* recibir_paquete(int);
void recibir_mensaje(int);
int recibir_operacion(int);

#endif /* UTILS_SERVER_H_ */
