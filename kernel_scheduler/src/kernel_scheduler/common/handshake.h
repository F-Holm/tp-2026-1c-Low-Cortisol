#pragma once

#include <stdbool.h>

#include "utils/log.h"

// Sends this module's handshake reply to a freshly accepted peer. Returns
// false (and logs) if the send fails.
bool respond_handshake(int socket_fd, int id_module, t_log* logger);
