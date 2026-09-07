# Operating System Simulator

A simplified take on a project that began as a *Trabajo Práctico* (course
assignment) for the Operating Systems course at UTN FRBA.

## Modules

The system is a distributed set of processes. Each module has its own README
describing what it does, how to run it, its configuration keys and its mandatory
logs.

| Module | Role |
|--------|------|
| [`kernel_scheduler`](kernel_scheduler/README.md) | Schedules Processes across the seven-state model. |
| [`kernel_memory`](kernel_memory/README.md) | Manages instruction/context memory, user memory and SWAP. |
| [`cpu`](cpu/README.md) | Runs the simplified instruction cycle. |
| [`memory_stick`](memory_stick/README.md) | A memory chip: serves reads and writes. |
| [`io`](io/README.md) | Simulates the `STDIN` / `STDOUT` / `SLEEP` interfaces. |
| [`swap`](swap/README.md) | Block store for suspended Processes. |
| [`utils`](utils/) | In-repo implementation of the shared utilities (lists, config, logging, sockets). |

## Dependencies

The project ships its own implementation of the utilities it needs in the
`utils` module. The only dependencies are system libraries (`pthread`,
`readline`, `m`), available in any standard GCC installation.

## Building and running

The repository has a top-level `Makefile` that drives every module.

```sh
make            # build every module in debug mode (same as `make debug`)
make release    # build every module in release mode
make <module>   # build a single module (cpu, io, kernel_memory, ...)
make clean      # remove object files and binaries
```

Each module's executable is written to `<module>/bin/<module>`.

Unit tests use [Criterion](https://criterion.readthedocs.io) and live under
`<module>/tests/`:

```sh
make test            # build + run every module's unit-test suite
make test-utils      # ... just one module's suite
```

To run the whole distributed system, use one of the end-to-end scenarios under
`tests/`:

```sh
make <scenario>               # build + launch a scenario
make <scenario> MODE=memcheck  # ... with every process under Valgrind
make full                     # launch the full-system scenario
make kill                     # stop every process
```

See [`tests/README.md`](tests/README.md) for the scenario list and port map, and
[`CONTRIBUTING.md`](CONTRIBUTING.md) for every `make` target and the code style.
