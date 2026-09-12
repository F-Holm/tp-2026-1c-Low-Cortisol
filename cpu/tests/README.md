# cpu unit tests

[Criterion](https://criterion.readthedocs.io) suites for the cpu module.

| File | Covers |
|------|--------|
| `registers_test.c` | `get_register`, `set_register` (`registers.h`) |
| `cpu_test.c` | `decode_stage` (`cpu.h`) |
| `kernel_memory_protocol_test.c` | `parse_stick_packet` (`kernel_memory_protocol.h`) |
| `handlers_test.c` | the register-only handlers: `handler_noop`, `handler_set`, `handler_sum`, `handler_sub`, `handler_jnz` (`handlers.h`) |
| `memory_test.c` | `find_segment_by_id`, `find_stick`, `mmu` (`memory.h`) |
| `connections_test.c` | `compute_offset` (`connections.h`) |
| `initializer_test.c` | `check_arguments`, `register_handlers` (`initializer.h`) |
| `cleanup_test.c` | `destroy_instruction`, `destroy_memory_stick`, `iterator_close_socket`, `close_module` (`cleanup.h`) |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-cpu      # from the repo root
```

## Not unit-tested

The rest of `cpu.h`, `connections.h`, `memory.h` and all of `handlers.h` beyond
the five register handlers drive the full instruction cycle: they exchange
context, instructions and memory reads/writes with Kernel Memory, Kernel
Scheduler and the memory sticks over sockets, in a loop. Those belong to the
end-to-end scenarios under `tests/`. Where a function's failure path only needs
one peer (`mmu` raising a segfault, for instance) it is covered here with a
loopback stub.
