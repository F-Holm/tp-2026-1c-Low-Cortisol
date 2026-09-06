# Operating System Simulator

A simplified take on a project that began as a *Trabajo Práctico* (course
assignment) for the Operating Systems course at UTN FRBA.

## Dependencies

The project ships its own implementation of the utilities it needs in the
`utils` module, so it no longer depends on the cátedra's `so-commons-library`.
The only dependencies are system libraries (`pthread`, `readline`, `m`),
available in any standard GCC installation.

## Building and running

The repository has a top-level `Makefile` that drives every module.

```sh
make            # build every module in debug mode (same as `make debug`)
make release    # build every module in release mode
make <module>   # build a single module (cpu, io, kernel_memory, ...)
make clean      # remove object files and binaries
```

Each module's executable is written to `<module>/bin/<module>`.

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
