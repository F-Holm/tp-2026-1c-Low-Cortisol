# Mid-term scheduling test (v2)

Maps to **Prueba Planificacion Mediano Plazo** in `../final-tests-guide.pdf`.

## What it exercises

Same as `pmp` with the alternative `PMP_v2.prc` workload and 1000 B memory sticks.

## Workload

`PMP_v2.prc` spawns `PMP_1..4.prc` spaced by `SLEEP 8000`, then exits.

## Key configuration

Same scheduler/memory config as `pmp`; 4 memory sticks (16, 16, 32, 64 B) with MEMORY_DELAY=1000.

## Expected result

Same as `pmp`.

## Run

```sh
make pmp-v2                 # kernel_memory :27174, kernel_scheduler :37023
make pmp-v2 MODE=memcheck
make kill
```
