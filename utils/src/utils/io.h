#pragma once

typedef enum
{
  IO_STDIN,
  IO_STDOUT,
  IO_SLEEP

} t_io_type;

extern const char* const IO_TYPE_NAMES[3];
