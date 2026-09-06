MODULES = cpu io kernel_memory kernel_scheduler memory_stick swap utils

# ─── End-to-end test scenarios ───────────────────────────────────────────────
#
# Every directory under tests/ (except pseudocode/) is a scenario. It holds
# the six <module>.conf files it runs with and a test.mk declaring its
# parameters. Launch one with `make <scenario>` (e.g. `make base`); add
# `MODE=memcheck` or `MODE=helgrind` to run every process under Valgrind.
# `make run` is an alias for `make full`. Stop everything with `make kill`.

E2E_TESTS := $(filter-out pseudocode,$(patsubst tests/%/,%,$(wildcard tests/*/)))

SLEEP_TIME  ?= 0.1
ESPERA_CPUS ?= 40
MODE        ?=

VALGRIND_memcheck := valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --errors-for-leak-kinds=all
VALGRIND_helgrind := valgrind --tool=helgrind --history-level=full --trace-children=yes
VALGRIND         := $(VALGRIND_$(MODE))

.PHONY: all debug release test clean logs format run kill $(E2E_TESTS) $(MODULES)

all: $(MODULES)

debug:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir DEBUG; \
	done

release:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir RELEASE; \
	done

test:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir test; \
	done

clean:
	@for dir in $(MODULES); do \
		$(MAKE) -C $$dir clean; \
	done

logs:
	@echo "Removing log files..."
	-rm -rf ./*.log ./*/*.log ./output
	@echo "Logs removed."

format:
	find . -iname "*.c" -o -iname "*.h" | grep -v "tests/" | xargs clang-format -i --style=file

# Load the parameters of the scenario being launched (`run` maps to `full`).
ACTIVE_TEST := $(firstword $(filter $(MAKECMDGOALS),$(E2E_TESTS)))
ifneq ($(filter run,$(MAKECMDGOALS)),)
ACTIVE_TEST := full
endif
ifneq ($(ACTIVE_TEST),)
include tests/$(ACTIVE_TEST)/test.mk
endif

run: full

$(E2E_TESTS): all logs
	@mkdir -p output
	@echo "Launching end-to-end test '$@'$(if $(MODE), [$(MODE)])..."
	$(VALGRIND) ./kernel_memory/bin/kernel_memory tests/$@/kernel_memory.conf > output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND) ./swap/bin/swap tests/$@/swap.conf > output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND) ./kernel_scheduler/bin/kernel_scheduler tests/$@/kernel_scheduler.conf $(INITIAL_PROCESS) > output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	@i=1; for size in $(STICK_SIZES); do \
		$(VALGRIND) ./memory_stick/bin/memory_stick tests/$@/memory_stick.conf $$size > output/memory_stick_$$i.log 2>&1 & \
		sleep $(SLEEP_TIME); i=$$((i + 1)); \
	done
	$(VALGRIND) ./io/bin/io tests/$@/io.conf SLEEP > output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND) ./io/bin/io tests/$@/io.conf STDIN < tests/pseudocode/stdin_input.txt > output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND) ./io/bin/io tests/$@/io.conf STDOUT > output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	@n=1; while [ $$n -le $(CPU_COUNT) ]; do \
		$(VALGRIND) ./cpu/bin/cpu tests/$@/cpu.conf CPU-$$n > output/cpu_$$n.log 2>&1 & \
		if [ "$(strip $(CPU_STAGGER))" = "$$n" ]; then sleep $(ESPERA_CPUS); else sleep $(SLEEP_TIME); fi; \
		n=$$((n + 1)); \
	done
	@echo "Launched. Logs in ./output/. Stop with 'make kill'."

kill:
	@echo "Stopping the system..."
	-pkill -f valgrind
	-pkill -f "./kernel_memory/bin/"
	-pkill -f "./kernel_scheduler/bin/"
	-pkill -f "./memory_stick/bin/"
	-pkill -f "./swap/bin/"
	-pkill -f "./io/bin/"
	-pkill -f "./cpu/bin/"

$(MODULES):
	$(MAKE) -C $@
