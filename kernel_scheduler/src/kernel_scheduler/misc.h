#ifndef KERNEL_SCHEDULER_MISC_H_
#define KERNEL_SCHEDULER_MISC_H_

#include <commons/log.h>
#include <stdbool.h>

bool responder_handshake(int socket_fd, int id_modulo, t_log* logger);

#endif /* KERNEL_SCHEDULER_MISC_H_ */