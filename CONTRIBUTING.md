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
| `make` / `make all` | Build every module (debug). |
| `make debug` / `make release` | Build every module in that mode. |
| `make <module>` | Build a single module, e.g. `make kernel_scheduler`. |
| `make <module> BUILD=release` | ... in release mode. |
| `make clean` | Remove the build tree and the module binaries. |
| `make logs` | Remove generated log files. |
| `make format` | Run clang-format over the whole codebase. |

Objects and the `utils` archive live under `build/<mode>/`; each module's
binary is written to `<module>/bin/<module>`.

### Unit tests

Unit tests use [Criterion](https://criterion.readthedocs.io) and live under
`<module>/tests/`. Each `.c` file there is a suite that is linked against its
module's own objects (minus `main.o`) and run as a standalone binary.

| Command | Description |
| :--- | :--- |
| `make test` | Build and run every module's suite. |
| `make test-<module>` | Build and run one module's suite, e.g. `make test-utils`. |

Criterion must be installed (`pacman -S criterion` on Arch,
`apt install libcriterion-dev` on Debian). `make clean` also removes the test
binaries. Test files follow the same `.clang-format` style as the rest of the
codebase.

### End-to-end tests

Every directory under `tests/` (except `pseudocode/`) is a scenario: it holds
its six `<module>.conf` files and a `test.mk` with its parameters.

| Command | Description |
| :--- | :--- |
| `make <scenario>` | Build and launch the given scenario. |
| `make <scenario> MODE=memcheck` | Same, with every process under Valgrind memcheck. |
| `make <scenario> MODE=helgrind` | Same, with every process under Valgrind helgrind. |
| `make kill` | Stop every process of the system. |

Available scenarios: `base`, `base2`, `short-term`, `mem-best`, `mem-worst`, `medium-term`,
`medium-term-det`, `medium-term-v2`, `medium-term-det-v2`, `priority-inheritance`, `priority-inheritance-v2`, `stability-1`, `stability-2`, `stability-3`,
`stability-4`, `full`.

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
