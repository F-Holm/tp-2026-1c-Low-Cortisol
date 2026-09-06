# Contributing guide

## 1. C code style

| Element | Example |
| :--- | :--- |
| **Constants and macros** | `ELEMENT_NAME` |
| **Variables and functions** | `element_name` |
| **Structs and typedefs** | `t_element_name` |
| **Headers** | `element_name.h` |
| **Source files** | `element_name.c` |

Formatting is enforced by `.clang-format`; run `make format` before committing.

---

## 2. Make targets

### Building

| Command | Description |
| :--- | :--- |
| `make` / `make all` | Same as `make debug`. |
| `make debug` | Build the whole project in debug mode. |
| `make release` | Build the whole project in release mode. |
| `make <module>` | Build a single module. |
| `make clean` | Remove object files and generated binaries. |
| `make logs` | Remove generated log files. |
| `make test` | Build and run each module's unit tests. |
| `make format` | Run clang-format over the whole codebase. |

### End-to-end tests

Every directory under `tests/` (except `pseudocode/`) is a scenario: it holds
its six `<module>.conf` files and a `test.mk` with its parameters.

| Command | Description |
| :--- | :--- |
| `make <scenario>` | Build and launch the given scenario. |
| `make <scenario> MODE=memcheck` | Same, with every process under Valgrind memcheck. |
| `make <scenario> MODE=helgrind` | Same, with every process under Valgrind helgrind. |
| `make run` | Alias for `make full`. |
| `make kill` | Stop every process of the system. |

Available scenarios: `base`, `base2`, `pcp`, `mem-best`, `mem-worst`, `pmp`,
`pmp-det`, `pmp-v2`, `pmp-det-v2`, `php`, `php-v2`, `es3-1`, `es3-2`, `es3-3`,
`es3-4`, `full`.

Each process's log is written to `./output/`.

---

## 3. Comments

Every function must be declared in a header with the same name as the source
file where it is implemented.

Add a comment in the format below before each function declaration in the
headers.

```c
/**
 * @brief Prints a greeting to the console
 * @param who Module the call comes from
 * @return Nothing
 * @note free the dynamic memory of ...
 */
void greet(char* who);
```
