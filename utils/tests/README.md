# utils unit tests

[Criterion](https://criterion.readthedocs.io) suites for the shared utilities.
Each file is one suite:

| File | Covers |
|------|--------|
| `string_test.c` | every function in `utils/string.h` |
| `list_test.c` | every function in `utils/collections/list.h` |
| `dictionary_test.c` | every function in `utils/collections/dictionary.h` |
| `config_test.c` | every function in `utils/config.h` |
| `log_test.c` | every function in `utils/log.h` |
| `msg_test.c` | every function in `utils/msg.h`, over a loopback socket pair |
| `io_test.c` | the `IO_TYPE_NAMES` table in `utils/io.h` |

`msg_test.c` opens a real TCP connection to `127.0.0.1` on an ephemeral port
(picked by the kernel, discovered with `getsockname`), so the suite needs no
fixed port and two suites can run at once. `syscalls.h`, `registers_cpu.h` and
`swap_km.h` are only type definitions and have nothing to test.

## Running

```sh
make test-utils      # from the repo root
```

The suite is compiled against `libutils.a` and linked with `-lcriterion`, so
Criterion has to be installed (`pacman -S criterion` on Arch,
`apt install libcriterion-dev` on Debian).

## Adding a test

Add a `Test(suite, name)` to the matching file, or a new `*_test.c` for a new
header. Criterion runs each test in its own process, so a global is reset to its
initial value before every test. These files are exempt from `make format`.
