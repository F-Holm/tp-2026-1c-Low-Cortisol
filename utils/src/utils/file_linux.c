#include "utils/os.h"

#ifdef OS_LINUX

#include <unistd.h>

#include "utils/file.h"

bool file_resize(FILE* file, long size)
{
  if (fflush(file) != 0)
    return false;
  return ftruncate(fileno(file), size) == 0;
}

#endif  // OS_LINUX
