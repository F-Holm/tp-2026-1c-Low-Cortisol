# kernel_memory unit tests

[Criterion](https://criterion.readthedocs.io) suites for the kernel_memory
module. The module is mostly threaded server code; these suites cover the
memory-management logic and the pure helpers that back it.

| File | Covers |
|------|--------|
| `configurator_test.c` | `allocation_from_string`, `get_scripts_basepath`, `get_instruction_delay`, `get_compaction_delay`, `get_segment_max_size`, `get_allocation_strategy`, `init_config` |
| `registry_test.c` | `registry`: `list_add_mtx`, `find_process` |
| `connections_test.c` | `connections`: `compute_total_memory` |
| `holes_test.c` | `holes`: `compute_free_space`, `add_total_memory`, `select_hole` (BEST / WORST / exact / none) |
| `compaction_test.c` | `compaction`: `compute_last_segment_end`, `compact_holes` |
| `segments_test.c` | `segments`: `hole_before_segment`, `hole_after_segment`, `compute_process_size` |
| `lifecycle_test.c` | `init_main_memory`, `init_cpu_data`, `init_stick_data`, `init_process`, `free_swap_data`, `free_main_memory` |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-kernel_memory      # from the repo root
```

## Not unit-tested

The socket protocol handlers (`create_segment`, `remove_segment`,
`translate_logical_address`, `read_from_sticks`, `write_to_sticks`,
`compact_memory`, `suspend_process`, `resume_process`, everything in
`scheduler_listener.h`, `cpu_listener.h` and `server.h`, …) coordinate the scheduler, the CPUs and the
memory sticks over sockets and mutate shared state under locks. They are
exercised by the end-to-end scenarios under `tests/` (`mem-best`, `mem-worst`,
`medium-term`, …).
