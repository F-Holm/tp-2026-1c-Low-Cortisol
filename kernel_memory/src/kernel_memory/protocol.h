#pragma once

// Umbrella header for the Kernel Memory protocol logic, split by concern:
#include "kernel_memory/address_translation.h"  // IWYU pragma: export
#include "kernel_memory/compaction.h"           // IWYU pragma: export
#include "kernel_memory/connections.h"          // IWYU pragma: export
#include "kernel_memory/holes.h"                // IWYU pragma: export
#include "kernel_memory/registry.h"             // IWYU pragma: export
#include "kernel_memory/segments.h"             // IWYU pragma: export
