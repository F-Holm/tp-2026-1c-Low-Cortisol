# End-to-end tests

Every directory here (except `pseudocode/`) is a **scenario**: a full run of the
distributed system with a fixed configuration and workload. Each scenario holds
its six `<module>.conf` files and a `test.mk` with its parameters (initial
process, memory stick sizes, CPU count, CPU stagger).

## Running

```sh
make <scenario>                 # build + launch
make <scenario> MODE=memcheck   # every process under Valgrind memcheck
make <scenario> MODE=helgrind   # every process under Valgrind helgrind
make kill                       # stop everything
```

Per-process logs are written to `./output/`. Each scenario has its own
`README.md` describing what it exercises and the expected result; those are
based on `final-tests-guide.pdf` (kept locally, not committed) and the
pseudocode each scenario runs.

## Ports

Each scenario uses its own TCP port pair so two scenarios can run at the same
time without colliding. The pair is `kernel_memory = 27167 + N`,
`kernel_scheduler = 37016 + N`, where `N` is the scenario's index below.

| N | scenario | kernel_memory | kernel_scheduler |
|---|---|---|---|
| 0 | base | 27167 | 37016 |
| 1 | base2 | 27168 | 37017 |
| 2 | pcp | 27169 | 37018 |
| 3 | mem-best | 27170 | 37019 |
| 4 | mem-worst | 27171 | 37020 |
| 5 | pmp | 27172 | 37021 |
| 6 | pmp-det | 27173 | 37022 |
| 7 | pmp-v2 | 27174 | 37023 |
| 8 | pmp-det-v2 | 27175 | 37024 |
| 9 | php | 27176 | 37025 |
| 10 | php-v2 | 27177 | 37026 |
| 11 | es3-1 | 27178 | 37027 |
| 12 | es3-2 | 27179 | 37028 |
| 13 | es3-3 | 27180 | 37029 |
| 14 | es3-4 | 27181 | 37030 |
| 15 | full | 27182 | 37031 |
