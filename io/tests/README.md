# io unit tests

[Criterion](https://criterion.readthedocs.io) suites for the io module. Each
file covers one source file:

| File | Covers |
|------|--------|
| `utils_test.c` | every function in `io/utils.h` (`parse_args`, `load_config`, `connect_to_scheduler`, `close_io`) |
| `ioops_test.c` | every function in `io/ioops.h` (`run_stdin`, `run_stdout`, `run_sleep`) |
| `support.c` | shared helpers (not a suite) |

## Running

```sh
make test-io      # from the repo root
```

## How the socket-facing code is tested

`connect_to_scheduler` and the request handlers talk to the Kernel Scheduler
over a socket, so the tests stand in for it:

- `support.c` opens a real loopback connection on a kernel-assigned ephemeral
  port (`io_connected_pair`), so no fixed port is needed.
- `connect_to_scheduler` is driven by a `scheduler_stub` thread that performs the
  handshake and records what io announced.
- The handlers assume `main()` already consumed the request op code, so each test
  reads it back before calling the handler.
- `run_stdin` reads the keyboard through `cr_redirect_stdin()`.
- A failed reply is forced deterministically with `shutdown(fd, SHUT_WR)` rather
  than by closing the peer (a single loopback `send()` to a closed peer often
  still succeeds).

`load_config` writes `io.log` in the working directory, so those tests `chdir`
into a throwaway directory first.
