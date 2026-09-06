#pragma once

#include <stdio.h>

#include "utils/collections/list.h"
#include "utils/registros_cpu.h"

typedef struct
{
  t_registros* registers;
  t_list* segment_table;
  bool segment_changed;
} t_context;

uint32_t get_register(t_registros* registers, char* register_name);
void set_register(t_registros* registers, char* register_name, uint32_t value);
