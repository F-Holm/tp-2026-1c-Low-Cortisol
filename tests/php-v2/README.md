# Priority inheritance test (v2)

Maps to **Prueba Herencia de Prioridades** in `../final-tests-guide.pdf`.

## What it exercises

Same as `php` with the `PHP_v2.prc` workload (2 mutexes, tighter `SLEEP`s).

## Workload

`PHP_v2.prc` creates `MUTEX_1/2`, then spawns `PHP_v2_1..4.prc` at priorities 5, 3, 2 and 1.

## Key configuration

Same as `php`.

## Expected result

Same as `php`.

## Run

```sh
make php-v2                 # kernel_memory :27177, kernel_scheduler :37026
make php-v2 MODE=memcheck
make kill
```
