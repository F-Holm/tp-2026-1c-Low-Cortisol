# kernel_scheduler unit tests

[Criterion](https://criterion.readthedocs.io) suites for the kernel_scheduler
module. The scheduler is almost entirely threaded state machines; these suites
cover the PCB bookkeeping, the priority ordering and the mutex-registry logic.

| File | Covers |
|------|--------|
| `names_test.c` | `STATE_NAMES`, `SCHEDULING_ALGORITHMS`, `SYSCALL_NAMES`, `PREEMPTION_REASONS` |
| `pcb_test.c` | `domain/pcb`: `create_pcb`, `destroy_pcb`, `get_state_pcb`, `get_priority_pcb`, `increment_active_instances`, `decrement_active_instances`, `set_mutex_blocking`, `get_mutex_blocking`, `insert_pcb_sorted` |
| `time_test.c` | `common/time`: `time_diff`, `millis` |
| `kernel_memory_socket_test.c` | `domain/kernel_memory_socket`: `init_socket_kernel_memory`, `destroy_kernel_memory` |
| `counter_test.c` | `scheduler/counter`: `create_counter`, `counter_increment`, `destroy_counter` |
| `process_counter_test.c` | `scheduler/process_counter`: increment / non-final decrement |
| `ready_queue_test.c` | `scheduler/ready_queue`: FIFO and CMN init, the preempt / terminate gates, `check_priority_valid`, put/take ordering, multilevel priority, blocking take |
| `blocking_list_test.c` | `scheduler/blocking_list`: FIFO `BLOCK`, priority-sorted `SUSP. BLOCK` / `SUSP. READY`, blocked-time stamping, take-specific / take-next / empty |
| `exec_list_test.c` | `scheduler/exec_list`: put/take, `transition_take_exec_next`, lowest-priority tracking with preemption, the non-blocking `wait_*` paths |
| `state_machine_test.c` | `scheduler/queues`: `transition_exec_ready` / `_exec_block` / `_exec_exit` / `_block_ready` / `_ready_exec`, wrong-state rejection, `clear_queues` draining every queue |
| `mutex_test.c` | `syscalls/mutex`: create / duplicate / unknown / grant / wrong-owner, FIFO hand-off, priority inheritance and its transitive propagation, revert on unlock |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-kernel_scheduler      # from the repo root
```

## Not unit-tested

Everything in `scheduler/queues.h` (the seven-state transitions, the suspension/resumption
routines, compaction), the `connections/` package (`cpu`, `io`, `server`,
`kernel_memory`) and `syscalls/memory`, plus `start_module` / `close_module`, is
threaded orchestration that
coordinates the CPUs, the IO interfaces and Kernel Memory over sockets. Those are
covered by the end-to-end scenarios under `tests/` (`short-term`, `medium-term`,
`priority-inheritance`, `stability-*`, …). The priority-inheritance paths of
`list_mutex_lock` need a fully built `t_queues`, so only the non-inheritance
paths are unit-tested here.
