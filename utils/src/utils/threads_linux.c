#include "utils/os.h"

#ifdef OS_LINUX

#include "utils/threads.h"

int thrd_create(thrd_t* thread, thrd_start_t start, void* arg)
{
  return pthread_create(thread, NULL, start, arg);
}

int thrd_join(thrd_t thread, void** result)
{
  return pthread_join(thread, result);
}

int thrd_detach(thrd_t thread)
{
  return pthread_detach(thread);
}

#endif  // OS_LINUX
