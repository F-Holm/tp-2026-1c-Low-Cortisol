#ifndef MEMORY_STICK_CPU_H_
#define MEMORY_STICK_CPU_H_

#include <stdint.h>

int create_server_cpu(void);
uint16_t get_puerto_cpu(int socket);
void* hilo_escucha_cpu(int socket);

#endif /* MEMORY_STICK_CPU_H_ */
