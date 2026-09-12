# kernel_scheduler unit tests

[Criterion](https://criterion.readthedocs.io) suites for the kernel_scheduler
module. The socket- and thread-free core -- the PCB, the queue data structures,
the state machine and the mutex registry -- is covered here; the parts that
talk to CPUs / IO / Kernel Memory over sockets are covered by the end-to-end
scenarios.

| File | Covers |
|------|--------|
| `names_test.c` | `STATE_NAMES`, `SCHEDULING_ALGORITHMS`, `SYSCALL_NAMES`, `PREEMPTION_REASONS` |
| `pcb_test.c` | `domain/pcb`: `create_pcb`, `destroy_pcb`, `get_state_pcb`, `get_priority_pcb`, `increment_active_instances`, `decrement_active_instances`, `set_mutex_blocking`, `get_mutex_blocking`, `insert_pcb_sorted` |
| `time_test.c` | `common/time`: `time_diff`, `millis` |
| `kernel_memory_socket_test.c` | `domain/kernel_memory_socket`: `init_socket_kernel_memory`, `destroy_kernel_memory` |
| `handshake_test.c` | `common/handshake`: `respond_handshake` over a live socket / a dead fd |
| `counter_test.c` | `scheduler/counter`: `create_counter`, `counter_increment`, `destroy_counter` |
| `process_counter_test.c` | `scheduler/process_counter`: increment / non-final decrement |
| `ready_queue_test.c` | `scheduler/ready_queue`: FIFO and CMN init, the preempt / terminate gates, `check_priority_valid`, put/take ordering, multilevel priority, blocking take |
| `blocking_list_test.c` | `scheduler/blocking_list`: FIFO `BLOCK`, priority-sorted `SUSP. BLOCK` / `SUSP. READY`, blocked-time stamping, take-specific / take-next / empty |
| `exec_list_test.c` | `scheduler/exec_list`: put/take, `transition_take_exec_next`, lowest-priority tracking with preemption, the non-blocking `wait_*` paths |
| `state_machine_test.c` | `scheduler/queues`: `transition_exec_ready` / `_exec_block` / `_exec_exit` / `_block_ready` / `_ready_exec` / `_susp_block`, `transition_unlock`, wrong-state rejection, the syscall counter, `clear_queues` draining every queue |
| `mutex_test.c` | `syscalls/mutex`: create / duplicate / unknown / grant / wrong-owner, FIFO hand-off, priority inheritance and its transitive propagation, revert on unlock |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-kernel_scheduler      # from the repo root
```

## Not unit-tested

What's left over talks to sockets or spawns threads, so it is exercised only by
the end-to-end scenarios under `tests/` (`short-term`, `medium-term`,
`priority-inheritance`, `stability-*`, …):

- `scheduler/suspension.c` and `scheduler/compaction.c` (the worker threads and
  the Kernel Memory suspend/resume/compact handshakes)
- `scheduler/memory_query.c` (the free-space / process-size requests)
- `scheduler/queues.c`'s `transition_new_ready` (needs Kernel Memory to admit
  the process)
- the whole `connections/` package and `syscalls/memory.c`
- `app/kernel_scheduler.c` (`start_module` / `close_module`)
