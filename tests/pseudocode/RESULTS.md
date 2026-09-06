# Expected results

## Final tests

See the course test guide (not committed) and the per-scenario `README.md` under
`tests/<scenario>/`.

## Preliminary scripts

A short description of what each preliminary script is meant to show.

### Short-term scheduling (`SCHED_PRE_*`)

Validates short-term process scheduling with nothing memory-related involved.
Start the whole system with a single CPU connected. If `SUSPENSION_TIMEOUT` is
high enough this case is never hit; to exercise suspension, set it below 20000
and at least the processes spawned from `SCHED_PRE_1.prc` should try to suspend.

### Preliminary memory (`MEM_PRE_*`)

A single 256-byte memory stick with `SEGMENT_MAX_SIZE=128` is enough. The script
creates segments, writes to them, reads them back and deletes some of them. You
can also run several smaller memory sticks to exercise segments that span more
than one stick. `MEM_PRE_3.prc` ends quickly with a segmentation fault.
