#ifndef UTILS_IO_H_
#define UTILS_IO_H_

#include <assert.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef enum
{
  E_STDIN,
  E_STDOUT,
  E_SLEEP

} t_tipo_io;

extern const char* const V_TIPO_IO[3];

#endif /* UTILS_MSG_ */
