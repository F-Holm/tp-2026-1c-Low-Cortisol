# kernel_scheduler unit tests

[Criterion](https://criterion.readthedocs.io) suites for the kernel_scheduler
module. The scheduler is almost entirely threaded state machines; these suites
cover the PCB bookkeeping, the priority ordering and the mutex-registry logic.

| File | Covers |
|------|--------|
| `names_test.c` | `STATE_NAMES`, `SCHEDULING_ALGORITHMS`, `SYSCALL_NAMES`, `PREEMPTION_REASONS` |
| `misc_test.c` | `create_pcb`, `destroy_pcb`, `get_state_pcb`, `get_priority_pcb`, `incrementar_instances_active_pcb`, `disminuir_instances_active_pcb`, `set_mutex_blocking`, `get_mutex_blocking`, `insert_pcb_in_orden`, `time_diff`, `millis`, `init_socket_kernel_memory`, `destroy_kernel_memory` |
| `mutex_test.c` | `init_list_mutex`, `destroy_list_mutex`, `create_and_add_mutex`, `list_mutex_lock`, `list_mutex_unlock` (create / duplicate / unknown / grant / wrong-owner) |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-kernel_scheduler      # from the repo root
```

## Not unit-tested

Everything in `queue.h` (the seven-state transitions, the suspension/resumption
routines, compaction), `cpu.h`, `io.h`, `server.h`, `kernel_memory.h` and
`memory.h`, plus `start_module` / `close_module`, is threaded orchestration that
coordinates the CPUs, the IO interfaces and Kernel Memory over sockets. Those are
covered by the end-to-end scenarios under `tests/` (`short-term`, `medium-term`,
`priority-inheritance`, `stability-*`, …). The priority-inheritance paths of
`list_mutex_lock` need a fully built `t_queues`, so only the non-inheritance
paths are unit-tested here.
