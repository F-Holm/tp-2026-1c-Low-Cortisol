#include "kernel_scheduler/domain/kernel_memory_socket.h"

#include <criterion/criterion.h>

Test(ks_kernel_memory_socket, init_socket_kernel_memory_wraps_the_fd)
{
  t_kernel_memory_socket* km = init_socket_kernel_memory(9);
  cr_assert_eq(km->km_socket, 9);
  destroy_kernel_memory(km);
}
