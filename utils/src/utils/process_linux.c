#include "utils/os.h"

#ifdef OS_LINUX

#include <sys/syscall.h>
#include <unistd.h>

#include "utils/process.h"

int process_get_id(void)
{
  return getpid();
}

long process_get_thread_id(void)
{
  return syscall(SYS_gettid);
}

#endif  // OS_LINUX
