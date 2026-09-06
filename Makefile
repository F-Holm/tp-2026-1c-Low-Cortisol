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
CFLAGS_debug   := -g -Wall -DDEBUG -fdiagnostics-color=always
CFLAGS_release := -O3 -Wall -DNDEBUG
CFLAGS   := $(CFLAGS_$(BUILD)) -fPIC
DEPFLAGS := -MMD -MP
LDLIBS   := -lpthread -lreadline -lm
CC       := gcc
AR       := ar

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

# compile any source; the module is the first path component of the stem
$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -I$(firstword $(subst /, ,$*))/src -Iutils/src -c -o $@ $<

-include $(shell [ -d $(OBJDIR) ] && find $(OBJDIR) -name '*.d')

clean:
	rm -rf build $(addsuffix /bin,$(BIN_MODULES)) \
	       $(addsuffix /obj,$(BIN_MODULES) utils) utils/lib

logs:
	@echo "Removing log files..."
	-rm -rf ./*.log ./*/*.log ./output
	@echo "Logs removed."

format:
	find . -iname "*.c" -o -iname "*.h" | grep -v "tests/" | xargs clang-format -i --style=file

# ─── End-to-end test scenarios ──────────────────────────────────────────────
#
# Every directory under tests/ (except pseudocode/) is a scenario. It holds
# the six <module>.conf files it runs with and a test.mk declaring its
# parameters. Launch one with `make <scenario>` (e.g. `make base`); add
# `MODE=memcheck` or `MODE=helgrind` to run every process under Valgrind.
# `make full` launches the full-system scenario. Stop everything with `make kill`.

E2E_TESTS := $(filter-out pseudocode,$(patsubst tests/%/,%,$(wildcard tests/*/)))

SLEEP_TIME       ?= 0.1
CPU_STAGGER_WAIT ?= 40
MODE             ?=

VALGRIND_memcheck := valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --errors-for-leak-kinds=all
VALGRIND_helgrind := valgrind --tool=helgrind --history-level=full --trace-children=yes
VALGRIND          := $(VALGRIND_$(MODE))

.PHONY: all debug release clean logs format kill utils $(BIN_MODULES) $(E2E_TESTS)

# Load the parameters of the scenario being launched.
ACTIVE_TEST := $(firstword $(filter $(MAKECMDGOALS),$(E2E_TESTS)))
ifneq ($(ACTIVE_TEST),)
include tests/$(ACTIVE_TEST)/test.mk
endif

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
		if [ "$(strip $(CPU_STAGGER))" = "$$n" ]; then sleep $(CPU_STAGGER_WAIT); else sleep $(SLEEP_TIME); fi; \
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
