# ─── Build ──────────────────────────────────────────────────────────────────
#
#   make [all]                build every module (debug)
#   make <module>             build one module      e.g. make kernel_scheduler
#   make debug | make release build everything in that mode
#   make <module> BUILD=release
#   make clean
#
# Objects and the utils archive live under build/<mode>/; every module's binary
# stays at <module>/bin/<module>.

BIN_MODULES := cpu io kernel_memory kernel_scheduler memory_stick swap

BUILD ?= debug
CC ?= gcc
AR := ar
# gnu23, not plain c23: this project is POSIX/Linux-only (pthreads, sockets,
# readline, ...), so the GNU extensions -- which pull in POSIX/BSD library
# declarations automatically -- are a better fit than strict ISO C.
C_STD ?= gnu23

CFLAGS_debug   := -g -Wall -DDEBUG -fdiagnostics-color=always
CFLAGS_release := -O3 -Wall -DNDEBUG
CFLAGS   := $(CFLAGS_$(BUILD)) -std=$(C_STD) -fPIC
DEPFLAGS := -MMD -MP
LDLIBS   := -lpthread -lreadline -lm

OBJDIR   := build/$(BUILD)
LIBUTILS := $(OBJDIR)/libutils.a

# objects of a module: build/<mode>/<module>/src/.../x.o
objs = $(patsubst %.c,$(OBJDIR)/%.o,$(shell find $(1)/src -name '*.c'))

all: $(BIN_MODULES)

debug:
	@$(MAKE) --no-print-directory all BUILD=debug
release:
	@$(MAKE) --no-print-directory all BUILD=release

# utils is a static library, linked into every binary
utils: $(LIBUTILS)
$(LIBUTILS): $(call objs,utils)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

# one binary per module
define module_rule
$(1): $(1)/bin/$(1)
$(1)/bin/$(1): $(call objs,$(1)) $(LIBUTILS)
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -o $$@ $$^ $(LDLIBS)
endef
$(foreach m,$(BIN_MODULES),$(eval $(call module_rule,$(m))))

# ─── Unit tests (Criterion) ─────────────────────────────────────────────────
#
#   make test          build + run every module's unit-test suite
#   make test-<module> build + run one module's suite   e.g. make test-utils
#
# A suite is every .c under <module>/tests/, linked against that module's own
# objects (minus main.o) and Criterion. Needs Criterion installed
# (Arch: `pacman -S criterion`; Debian: `apt install libcriterion-dev`).

UNIT_MODULES := $(patsubst %/tests,%,$(wildcard $(addsuffix /tests,utils $(BIN_MODULES))))

CRITERION_CFLAGS := $(shell pkg-config --cflags criterion 2>/dev/null)
CRITERION_LIBS   := $(shell pkg-config --libs criterion 2>/dev/null || echo -lcriterion)

# a suite's own objects: build/<mode>/<module>/tests/.../x.o
test_objs = $(patsubst %.c,$(OBJDIR)/%.o,$(shell find $(1)/tests -name '*.c'))
# the module code under test: its objects except the entry point. utils has no
# entry point and is pulled from libutils.a instead, so it contributes nothing.
code_objs = $(if $(filter utils,$(1)),,$(filter-out $(OBJDIR)/$(1)/src/main.o,$(call objs,$(1))))

# suite objects also need the Criterion headers
$(foreach m,$(UNIT_MODULES),$(eval $(OBJDIR)/$(m)/tests/%.o: CFLAGS += $(CRITERION_CFLAGS)))

define unit_test_rule
test-$(1): $(1)/bin/$(1)_test | criterion-check
	@echo "Running '$(1)' unit tests..."
	timeout --foreground -k5 120 ./$$< || \
	  { echo "'$(1)' unit tests timed out or crashed the whole binary" >&2; exit 1; }
$(1)/bin/$(1)_test: $$(call test_objs,$(1)) $$(call code_objs,$(1)) $$(LIBUTILS)
	@mkdir -p $$(@D)
	$(CC) $(CFLAGS) -o $$@ $$^ $(LDLIBS) $$(CRITERION_LIBS)
endef
$(foreach m,$(UNIT_MODULES),$(eval $(call unit_test_rule,$(m))))

test: $(addprefix test-,$(UNIT_MODULES))

criterion-check:
	@pkg-config --exists criterion 2>/dev/null || { \
	  echo "Criterion not found. Install it (Arch: pacman -S criterion; Debian: apt install libcriterion-dev)."; \
	  exit 1; }

# compile any source; the module is the first path component of the stem
$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -I$(firstword $(subst /, ,$*))/src -Iutils/src -c -o $@ $<

-include $(shell [ -d $(OBJDIR) ] && find $(OBJDIR) -name '*.d')

clean:
	rm -rf build $(addsuffix /bin,$(BIN_MODULES) $(UNIT_MODULES)) \
	       $(addsuffix /obj,$(BIN_MODULES) utils) utils/lib

logs:
	@echo "Removing log files..."
	-rm -rf ./*.log ./*/*.log ./output
	@echo "Logs removed."

format:
	find . -iname "*.c" -o -iname "*.h" | xargs clang-format -i --style=file

# ─── End-to-end test scenarios ──────────────────────────────────────────────
#
# Every directory under tests/ (except pseudocode/) is a scenario. It holds
# the six <module>.conf files it runs with and a test.mk declaring its
# parameters, including TERMINATION and TIMEOUT (see tests/README.md).
#
#   make <scenario>                 build + launch, logs/pids in output/<scenario>/
#   make <scenario> MODE=memcheck   every process under Valgrind memcheck
#   make <scenario> MODE=helgrind   every process under Valgrind helgrind
#   make kill                       stop every module process on the machine
#   make e2e                        build, then run every scenario via tests/run_e2e.py
#
# `make <scenario>-run` launches the same scenario without the `all` build
# dependency and without touching any other scenario's output/ — that's what
# tests/run_e2e.py uses to run scenarios concurrently. `make kill` is
# machine-wide (fine for interactive use); the runner tears down each
# scenario by the PIDs it wrote to output/<scenario>/*.pid instead, so
# parallel scenarios never kill each other.

E2E_TESTS     := $(filter-out pseudocode,$(patsubst tests/%/,%,$(wildcard tests/*/)))
E2E_RUN_TESTS := $(addsuffix -run,$(E2E_TESTS))

SLEEP_TIME       ?= 0.1
CPU_STAGGER_WAIT ?= 40
MODE             ?=

VALGRIND_memcheck := valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --errors-for-leak-kinds=all
VALGRIND_helgrind := valgrind --tool=helgrind --history-level=full --trace-children=yes
VALGRIND          := $(VALGRIND_$(MODE))

.PHONY: all debug release clean logs format kill utils test criterion-check e2e \
        $(BIN_MODULES) $(E2E_TESTS) $(E2E_RUN_TESTS) $(addprefix test-,$(UNIT_MODULES))

# Load the parameters of the scenario being launched, for both
# `make <scenario>` and `make <scenario>-run`.
ACTIVE_TEST := $(firstword $(filter $(E2E_TESTS),$(patsubst %-run,%,$(MAKECMDGOALS))))
ifneq ($(ACTIVE_TEST),)
include tests/$(ACTIVE_TEST)/test.mk
endif

# Launches the 7-process system for scenario $(1); logs and PID files land in
# output/$(1)/, which the caller is expected to have created already.
define launch_scenario
	@echo "Launching end-to-end test '$(1)'$(if $(MODE), [$(MODE)])..."
	@{ $(VALGRIND) ./kernel_memory/bin/kernel_memory tests/$(1)/kernel_memory.conf > output/$(1)/kernel_memory.log 2>&1 & echo $$! > output/$(1)/kernel_memory.pid; }
	@sleep $(SLEEP_TIME)
	@{ $(VALGRIND) ./swap/bin/swap tests/$(1)/swap.conf > output/$(1)/swap.log 2>&1 & echo $$! > output/$(1)/swap.pid; }
	@sleep $(SLEEP_TIME)
	@{ $(VALGRIND) ./kernel_scheduler/bin/kernel_scheduler tests/$(1)/kernel_scheduler.conf $(INITIAL_PROCESS) > output/$(1)/kernel_scheduler.log 2>&1 & echo $$! > output/$(1)/kernel_scheduler.pid; }
	@sleep $(SLEEP_TIME)
	@i=1; for size in $(STICK_SIZES); do \
		{ $(VALGRIND) ./memory_stick/bin/memory_stick tests/$(1)/memory_stick.conf $$size > output/$(1)/memory_stick_$$i.log 2>&1 & echo $$! > output/$(1)/memory_stick_$$i.pid; }; \
		sleep $(SLEEP_TIME); i=$$((i + 1)); \
	done
	@{ $(VALGRIND) ./io/bin/io tests/$(1)/io.conf SLEEP > output/$(1)/io_sleep.log 2>&1 & echo $$! > output/$(1)/io_sleep.pid; }
	@sleep $(SLEEP_TIME)
	@{ $(VALGRIND) ./io/bin/io tests/$(1)/io.conf STDIN < tests/pseudocode/stdin_input.txt > output/$(1)/io_stdin.log 2>&1 & echo $$! > output/$(1)/io_stdin.pid; }
	@sleep $(SLEEP_TIME)
	@{ $(VALGRIND) ./io/bin/io tests/$(1)/io.conf STDOUT > output/$(1)/io_stdout.log 2>&1 & echo $$! > output/$(1)/io_stdout.pid; }
	@sleep $(SLEEP_TIME)
	@n=1; while [ $$n -le $(CPU_COUNT) ]; do \
		{ $(VALGRIND) ./cpu/bin/cpu tests/$(1)/cpu.conf CPU-$$n > output/$(1)/cpu_$$n.log 2>&1 & echo $$! > output/$(1)/cpu_$$n.pid; }; \
		if [ "$(strip $(CPU_STAGGER))" = "$$n" ]; then sleep $(CPU_STAGGER_WAIT); else sleep $(SLEEP_TIME); fi; \
		n=$$((n + 1)); \
	done
	@echo "Launched. Logs in output/$(1)/. Stop with 'make kill'."
endef

# `make <scenario>`: build everything, wipe ALL scenarios' output, launch this
# one. Meant for interactive, one-at-a-time use.
$(E2E_TESTS): all logs
	@mkdir -p output/$@
	$(call launch_scenario,$@)

# `make <scenario>-run`: launch only (binaries must already be built), touches
# only this scenario's own output dir. Safe to run several of these at once.
$(E2E_RUN_TESTS): %-run:
	@rm -rf output/$*
	@mkdir -p output/$*
	$(call launch_scenario,$*)

kill:
	@echo "Stopping the system..."
	-pkill -f valgrind
	-pkill -f "./kernel_memory/bin/"
	-pkill -f "./kernel_scheduler/bin/"
	-pkill -f "./memory_stick/bin/"
	-pkill -f "./swap/bin/"
	-pkill -f "./io/bin/"
	-pkill -f "./cpu/bin/"

e2e: all
	python3 tests/run_e2e.py
