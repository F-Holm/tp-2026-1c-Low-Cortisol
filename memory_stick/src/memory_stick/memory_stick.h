#ifndef MEMORY_STICK_MEMORY_STICK_H_
#define MEMORY_STICK_MEMORY_STICK_H_

#include <commons/config.h>
#include <stdbool.h>
#include <stdio.h>

const char *const CONFIG_FILE_NAME = "memory_stick.config";

typedef struct
{
  char *log_level;
  int memory_delay;
  char *ip_km;
  char *puerto_km;
} t_config_vars;

bool open_confir_ms(t_config *config);
void read_confir_ms(t_config *config, t_config_vars *config_vars);
void close_confir_ms(t_config *config);

#endif /* MEMORY_STICK_MEMORY_STICK_H_ */
