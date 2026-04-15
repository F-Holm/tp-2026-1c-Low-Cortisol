#ifndef MEMORY_STICK_MEMORY_STICK_H_
#define MEMORY_STICK_MEMORY_STICK_H_

#include <commons/config.h>
#include <stdio.h>

typedef struct
{
  char *log_level;
  int memory_delay;
  char *ip_km;
  char *puerto_km;
} t_config_vars;

void read_confir_ms(t_config *config, t_config_vars *config_vars);

#endif /* MEMORY_STICK_MEMORY_STICK_H_ */
