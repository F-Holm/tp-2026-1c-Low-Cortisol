MODULES = cpu io kernel_memory kernel_scheduler memory_stick swap utils
SLEEP_TIME = 0.1

.PHONY: all debug release test clean logs format run kill memcheck helgrind $(MODULES)

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
	@echo "Eliminando archivos de log..."
	-rm -rf ./*.log ./*/*.log ./output
	@echo "Logs eliminados correctamente."

format:
	find . -iname "*.c" -o -iname "*.h" | grep -v "tests/" | xargs clang-format -i --style=file


VALGRIND_MEMCHECK = valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --errors-for-leak-kinds=all
VALGRIND_HELGRIND = valgrind --tool=helgrind --history-level=full --trace-children=yes
VALGRIND_CMD = 
BUILD_TARGET = all

memcheck: BUILD_TARGET = debug
memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
memcheck: execute

helgrind: BUILD_TARGET = debug
helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
helgrind: execute

run: BUILD_TARGET = all
run: VALGRIND_CMD =
run: execute

execute: $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/kernel_memory.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernel_scheduler.config proceso_inicial.asm > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 1000 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 2000 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 3000 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-2 > ./output/cpu_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-3 > ./output/cpu_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 4000 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 5000 > ./output/memory_stick_5.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 6000 > ./output/memory_stick_6.log 2>&1 &
	
	@echo "Sistema lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

kill:
	@echo "Cerrando el sistema..."
	-pkill -f valgrind
	-pkill -f "./kernel_memory/bin/"
	-pkill -f "./kernel_scheduler/bin/"
	-pkill -f "./memory_stick/bin/"
	-pkill -f "./swap/bin/"
	-pkill -f "./io/bin/"
	-pkill -f "./cpu/bin/"

$(MODULES):
	$(MAKE) -C $@
