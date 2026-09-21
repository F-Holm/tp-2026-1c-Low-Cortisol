#include "utils/os.h"

#ifdef OS_LINUX

#include "utils/mutex.h"

int mtx_init(mtx_t* mutex)
{
  return pthread_mutex_init(mutex, NULL);
}

int mtx_lock(mtx_t* mutex)
{
  return pthread_mutex_lock(mutex);
}

int mtx_unlock(mtx_t* mutex)
{
  return pthread_mutex_unlock(mutex);
}

void mtx_destroy(mtx_t* mutex)
{
  pthread_mutex_destroy(mutex);
}

int cnd_init(cnd_t* cond)
{
  return pthread_cond_init(cond, NULL);
}

int cnd_wait(cnd_t* cond, mtx_t* mutex)
{
  return pthread_cond_wait(cond, mutex);
}

int cnd_signal(cnd_t* cond)
{
  return pthread_cond_signal(cond);
}

int cnd_broadcast(cnd_t* cond)
{
  return pthread_cond_broadcast(cond);
}

void cnd_destroy(cnd_t* cond)
{
  pthread_cond_destroy(cond);
}

#endif  // OS_LINUX
