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

---

## 4. Commit messages

Commit messages follow [Conventional Commits](https://www.conventionalcommits.org)
and are written in English.

```
<type>(<scope>): <summary>

<optional body>
```

- **type** — one of:

  | type | when |
  | :--- | :--- |
  | `feat` | new functionality |
  | `fix` | bug fix |
  | `refactor` | behaviour-preserving code change |
  | `perf` | performance improvement |
  | `style` | formatting only (no code change) |
  | `docs` | documentation only |
  | `test` | tests or test fixtures |
  | `build` | Makefile, build flags, tooling |
  | `chore` | anything else (repo housekeeping) |

- **scope** *(optional)* — the module or area touched: `utils`, `cpu`,
  `kernel_scheduler`, … Omit it when the change is repo-wide.
- **summary** — imperative mood, lowercase, no trailing period, ≤ 72 characters
  (`add own dictionary implementation`, not `Added a dictionary` or
  `adds dictionary.`).
- **body** *(optional)* — separated by a blank line, wrapped at ~72 columns.
  Explain *what* changed and *why*, not *how* (the diff already shows how).

Keep each commit to **one logical change** so history stays bisectable; split
unrelated changes into separate commits.

Examples:

```
feat(utils): add own linked list implementation
fix(kernel-scheduler): fix transitive priority inheritance
refactor: drop the SO commons library in favor of utils
docs: translate CONTRIBUTING to English
```
