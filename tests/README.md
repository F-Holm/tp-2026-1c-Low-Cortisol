# End-to-end tests

Every directory here (except `pseudocode/`) is a **scenario**: a full run of the
distributed system with a fixed configuration and workload. Each scenario holds
its six `<module>.conf` files and a `test.mk` with its parameters (initial
process, memory stick sizes, CPU count, CPU stagger, plus `TERMINATION` and
`TIMEOUT` -- see below).

## Running by hand

```sh
make <scenario>                 # build + launch
make <scenario> MODE=memcheck   # every process under Valgrind memcheck
make <scenario> MODE=helgrind   # every process under Valgrind helgrind
make kill                       # stop everything, machine-wide
```

Per-process logs and PID files are written to `output/<scenario>/`. Each
scenario has its own `README.md` describing what it exercises and the
expected result; those are based on the course test guide (not committed to
the repo) and the pseudocode each scenario runs.

## Running automatically

```sh
make e2e                              # build once, run every scenario, report pass/fail
python3 tests/run_e2e.py              # same, run directly
python3 tests/run_e2e.py --scenario base mem-best
python3 tests/run_e2e.py --jobs 1     # sequential instead of the default pool of 4
python3 tests/run_e2e.py --mode memcheck
python3 tests/run_e2e.py --list       # print each scenario's parsed parameters, run nothing
```

`run_e2e.py` builds once (`make all`), then launches scenarios concurrently
via `make <scenario>-run` (the same launch as `make <scenario>`, but without
the build dependency and without touching any other scenario's `output/`).
For each scenario it:

1. Polls `output/<scenario>/kernel_scheduler.log` for a `Shutting down: ...`
   line, up to `TIMEOUT` seconds.
2. If it's a `finite` scenario and that line never shows up, the scenario
   **fails immediately** -- that's treated as a hang (deadlock or similar),
   nothing is forced.
3. If it's an `infinite` scenario, reaching `TIMEOUT` without shutting down
   is expected: the runner kills `memory_stick_1` outright (simulating it
   being unplugged) to force Kernel Memory to report corrupted memory, and
   gives Kernel Scheduler `TIMEOUT_BSOD` more seconds to shut down with the
   BSOD reason. Still hanging after that is a real failure.
4. Evaluates every condition in `checks.txt` (see below) against all of the
   scenario's logs.
5. Under `--mode`, also requires `ERROR SUMMARY: 0 errors` in every log.
6. Tears the scenario down by the PIDs it recorded in
   `output/<scenario>/*.pid` (SIGTERM, then SIGKILL after a short grace
   period) -- never `make kill`, so scenarios running in parallel don't kill
   each other.

Exit code is the number of failed scenarios (0 = all green).

### `test.mk`: TERMINATION and TIMEOUT

```make
TERMINATION := finite      # or: infinite
TIMEOUT     := 60          # seconds
TIMEOUT_BSOD := 30          # infinite scenarios only: grace period after the forced disconnect
```

`stability-4`, `short-term`, `priority-inheritance`, `priority-inheritance-v2`
and `stability-{1,2,3}` are `infinite`: their workload includes a process
whose `EXIT` is unreachable (a `SET PC 0` loop, or a mutex/queue interaction
that's meant to keep the system busy indefinitely), by design, to soak-test
the system rather than run it to a clean finish. `stability-{1,2,3}` inherit
this from spawning `SHORT_TERM.prc` (and/or `PRIORITY_INHERITANCE.prc`) as
part of their combined workload. Everything else is `finite`.

`TIMEOUT` values are conservative starting points, not measured run times --
most workloads here chain several real `SLEEP`s (some scenarios reuse
pseudocode with two 20s `SLEEP`s run through a single shared `SLEEP` IO
device, so they serialize) plus per-instruction round trips over real
sockets, so scenarios comfortably take tens of seconds to a few minutes. Only
`base` was actually run to completion while sizing these; tune the rest down
once you've watched them run for real.

### `checks.txt`: pass conditions

One condition per line: a comparison operator (`==`, `<`, `>`, `<=`, `>=`), a
count, and a regex. The regex is matched against every line of every log the
scenario produced; the count is how many lines matched.

```
==1  Shutting down: Processes finished successfully
==0  Shutting down: BSOD: Corruption of memory detected
==0  \[ERROR\]
>=1  Change of priority:
```

Every finite scenario checks for the shutdown line, the absence of BSOD, and
the absence of any `[ERROR]`-level log; scenarios add checks for the
behaviour their README says they exercise (priority changes, compaction,
the resume-suspension routine). Every scenario's `*.conf` runs at
`LOG_LEVEL=TRACE` -- the lowest level, so nothing (compaction, the
resume-suspension routine, or anything else) is filtered out of the logs
these checks (or a human) read.

### Known limitations

Each module's own logger (`t_log`) always additionally writes to a
fixed-name file (`kernel_scheduler.log`, `cpu.log`, ...) at the repo root,
regardless of scenario -- that's separate from the `output/<scenario>/*.log`
files the harness reads (which come from redirecting each process's stdout,
and are already scenario-scoped) and isn't touched by any of this. Running
scenarios in parallel will interleave those root-level files; harmless for
the checks here, but worth a cleanup if it ever gets in the way.

## Ports

Each scenario uses its own TCP port pair so two scenarios can run at the same
time without colliding. The pair is `kernel_memory = 27167 + N`,
`kernel_scheduler = 37016 + N`, where `N` is the scenario's index below.

| N | scenario | kernel_memory | kernel_scheduler |
|---|---|---|---|
| 0 | base | 27167 | 37016 |
| 1 | base2 | 27168 | 37017 |
| 2 | short-term | 27169 | 37018 |
| 3 | mem-best | 27170 | 37019 |
| 4 | mem-worst | 27171 | 37020 |
| 5 | medium-term | 27172 | 37021 |
| 6 | medium-term-det | 27173 | 37022 |
| 7 | medium-term-v2 | 27174 | 37023 |
| 8 | medium-term-det-v2 | 27175 | 37024 |
| 9 | priority-inheritance | 27176 | 37025 |
| 10 | priority-inheritance-v2 | 27177 | 37026 |
| 11 | stability-1 | 27178 | 37027 |
| 12 | stability-2 | 27179 | 37028 |
| 13 | stability-3 | 27180 | 37029 |
| 14 | stability-4 | 27181 | 37030 |
| 15 | full | 27182 | 37031 |
