#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/registers_cpu.h"

typedef struct
{
  uint32_t pid;
  uint32_t bytes_to_read;
  uint32_t logical_address;
} t_stdin_request;

typedef struct
{
  uint32_t pid;
  uint32_t bytes_to_write;
  uint32_t logical_address;
} t_stdout_request;

typedef struct
{
  uint32_t pid;
  uint32_t blocked_time_ms;
} t_sleep_request;

typedef struct
{
  uint32_t pid;
  uint32_t segment_id;
  uint32_t size;
} t_syscall_memory;
