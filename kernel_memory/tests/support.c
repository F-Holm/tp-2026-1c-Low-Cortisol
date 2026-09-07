#include "support.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "utils/log.h"

t_log* km_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "KM-test", false, LOG_LEVEL_ERROR, true);
  cr_assert_not_null(logger);
  return logger;
}

t_hole* km_make_hole(int base, int size)
{
  t_hole* hole = malloc(sizeof(t_hole));
  hole->base = base;
  hole->size = size;
  return hole;
}

t_segment* km_make_segment(uint32_t id, uint32_t pid, int base, int size)
{
  t_segment* segment = calloc(1, sizeof(t_segment));
  segment->id = id;
  segment->pid = pid;
  segment->base = base;
  segment->size = size;
  return segment;
}

t_stick_data* km_make_stick(int size)
{
  t_stick_data* stick = calloc(1, sizeof(t_stick_data));
  stick->stick_size = size;
  return stick;
}
