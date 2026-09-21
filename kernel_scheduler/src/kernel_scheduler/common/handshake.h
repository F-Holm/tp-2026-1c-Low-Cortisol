#pragma once

#include <stdbool.h>

#include "utils/log.h"
#include "utils/sockets.h"

/**
 * @brief Sends this module's handshake reply to a freshly accepted peer.
 * @return false (and logs) if the send fails.
 */
bool respond_handshake(t_socket* socket_fd, int id_module, t_log* logger);
