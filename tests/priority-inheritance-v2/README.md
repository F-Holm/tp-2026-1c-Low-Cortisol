# Priority inheritance test (v2)

Covers the **Priority Inheritance** case from the course test guide (not committed to the repo).

## What it exercises

Same as `priority-inheritance` with the `PRIORITY_INHERITANCE_V2.prc` workload (2 mutexes, tighter `SLEEP`s).

## Workload

`PRIORITY_INHERITANCE_V2.prc` creates `MUTEX_1/2`, then spawns `PRIORITY_INHERITANCE_V2_1..4.prc` at priorities 5, 3, 2 and 1.

## Key configuration

Same as `priority-inheritance`.

## Expected result

Same as `priority-inheritance`.

## Run

```sh
make priority-inheritance-v2                 # kernel_memory :27177, kernel_scheduler :37026
make priority-inheritance-v2 MODE=memcheck
make kill
```
