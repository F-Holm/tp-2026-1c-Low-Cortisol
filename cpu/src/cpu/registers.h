#pragma once

#include <stdio.h>

#include "utils/collections/list.h"
#include "utils/registers_cpu.h"

typedef struct
{
  t_registers* registers;
  t_list* segment_table;
  bool segment_changed;
} t_context;

/** @brief Returns the value of @p register_name (e.g. "AX", "PC"). @return 0 if
 * unknown. */
uint32_t get_register(t_registers* registers, char* register_name);

/** @brief Sets @p register_name to @p value. No-op if unknown. */
void set_register(t_registers* registers, char* register_name, uint32_t value);
