# memory_stick unit tests

[Criterion](https://criterion.readthedocs.io) suites for the memory_stick module.

| File | Covers |
|------|--------|
| `memory_stick_test.c` | `get_args`, `read_config`, `init_config`, `write_memory`, `read_memory`, `close_module_on_error` (`memory_stick.h`) |
| `cpu_test.c` | `create_server_cpu`, `get_cpu_port`, `create_cpu_thread_data`, `iterator_shutdown`, `handshake_cpu`, `receive_cpu_id` (`cpu.h`) |
| `kernel_memory_test.c` | `send_size`, `send_cpu_server_port_to_km`, `connect_km`, `handshake_km`, `connect_to_kernel_memory` (`kernel_memory.h`) |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-memory_stick      # from the repo root
```

## Notes

- `write_memory` / `read_memory` run against a real `t_ms` (zeroed memory buffer,
  initialised mutex) and a loopback socket; the delay is set to zero.
- The Kernel Memory / CPU handshakes are driven by peer threads over a loopback
  connection. The stub thread arms `SO_RCVTIMEO` so a failed handshake on the
  module side cannot leave it blocked in `receive_op_code()`.
- `init_logger`, `send_cpu_server_port` and everything in `cpu.h` past the
  handshake (`spawn_cpu_thread`, `cpu_listen_thread`, `handle_cpu_client`,
  `handle_new_cpu`, `start_cpu_server`, `close_listen_thread`, `close_cpu_thread`)
  plus `init_module` / `close_module` are threaded server orchestration and
  belong to the end-to-end scenarios under `tests/`.

`get_args` is checked for the argument count, the extraction and the
non-positive-size rejection.
