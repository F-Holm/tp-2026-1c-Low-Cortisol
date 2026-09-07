# swap unit tests

[Criterion](https://criterion.readthedocs.io) suites for the swap module. The
module has a single source file, so the suites are split by concern:

| File | Covers |
|------|--------|
| `blocks_test.c` | `write_block`, `read_block` |
| `lifecycle_test.c` | `init_config`, `connect_to_kernel_memory`, `close_swap` |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-swap      # from the repo root
```

## Notes

- `blocks_test.c` works on a `tmpfile()` truncated to a few blocks and checks
  that blocks are addressed at `block_number * block_size` and do not overlap.
  It also pins the current `read_block` behaviour on a short read (the buffer is
  left untouched).
- `connect_to_kernel_memory` is driven by a `km_stub` thread that performs the
  handshake and reads back the `OP_INFO_SWAP` sizes, over a loopback connection
  on a kernel-assigned ephemeral port (`support.c`).
- `init_config` writes `swap.log` in the working directory, so those tests
  `chdir` into a throwaway directory first.
- Both `init_config` failure paths are covered (the log file cannot be opened,
  and the SWAP file cannot be created). The second one exercises `close_swap`
  before the socket or the SWAP file exist, so it also guards the fix that made
  `close_swap` release each resource only when it was acquired.
