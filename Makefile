MODULES = cpu io kernel_memory kernel_scheduler memory_stick swap utils
SLEEP_TIME = 0.1
ESPERA_CPUS = 40

.PHONY: all debug release test clean logs format run kill memcheck helgrind base base-memcheck base-helgrind base2 base2-memcheck base2-helgrind pcp pcp-memcheck pcp-helgrind mem mem-memcheck mem-helgrind mem2 mem2-memcheck mem2-helgrind pmp pmp-memcheck pmp-helgrind pmpdet pmpdet-memcheck pmpdet-helgrind pmp2 pmp2-memcheck pmp2-helgrind pmpdet2 pmpdet2-memcheck pmpdet2-helgrind php php-memcheck php-helgrind php2 php2-memcheck php2-helgrind es31 es31-mecheck es31-helgrind es32 es32-mecheck es32-helgrind es33 es33-mecheck es33-helgrind es34 es34-mecheck es34-helgrind $(MODULES)

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

# --- Modos de ejecución estándar ---
memcheck: BUILD_TARGET = debug
memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
memcheck: execute

helgrind: BUILD_TARGET = debug
helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
helgrind: execute

run: BUILD_TARGET = all
run: VALGRIND_CMD =
run: execute

execute: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/kernel_memory.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernel_scheduler.config proceso_inicial.asm > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 1000 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 2000 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 3000 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
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

# --- Modos de ejecución base ---
base: BUILD_TARGET = all
base: VALGRIND_CMD =
base: executebase

base-memcheck: BUILD_TARGET = debug
base-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
base-memcheck: executebase

base-helgrind: BUILD_TARGET = debug
base-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
base-helgrind: executebase

executebase: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/base.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/base.config PLANI_PRE_0.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 256 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución base2 ---
base2: BUILD_TARGET = all
base2: VALGRIND_CMD =
base2: executebase2

base2-memcheck: BUILD_TARGET = debug
base2-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
base2-memcheck: executebase2

base2-helgrind: BUILD_TARGET = debug
base2-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
base2-helgrind: executebase2

executebase2: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/base.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/base.config MEMORIA_PRE_0.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 256 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución PCP ---
pcp: BUILD_TARGET = all
pcp: VALGRIND_CMD =
pcp: executepcp

pcp-memcheck: BUILD_TARGET = debug
pcp-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
pcp-memcheck: executepcp

pcp-helgrind: BUILD_TARGET = debug
pcp-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
pcp-helgrind: executepcp

executepcp: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/pcp.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/pcp.config PCP.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 256 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución MEM ---
mem: BUILD_TARGET = all
mem: VALGRIND_CMD =
mem: executemem

mem-memcheck: BUILD_TARGET = debug
mem-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
mem-memcheck: executemem

mem-helgrind: BUILD_TARGET = debug
mem-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
mem-helgrind: executemem

executemem: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/mem_best.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/mem.config PLANI_MEM.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 32 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 64 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 128 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución MEM2 ---
mem2: BUILD_TARGET = all
mem2: VALGRIND_CMD =
mem2: executemem2

mem2-memcheck: BUILD_TARGET = debug
mem2-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
mem2-memcheck: executemem2

mem2-helgrind: BUILD_TARGET = debug
mem2-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
mem2-helgrind: executemem2

executemem2: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/mem_worst.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/mem.config PLANI_MEM.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 32 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 64 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 128 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución PMP ---
pmp: BUILD_TARGET = all
pmp: VALGRIND_CMD =
pmp: executepmp

pmp-memcheck: BUILD_TARGET = debug
pmp-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
pmp-memcheck: executepmp

pmp-helgrind: BUILD_TARGET = debug
pmp-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
pmp-helgrind: executepmp

executepmp: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/pmp.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/pmp.config PMP.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 32 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 64 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución PMP-DET ---
pmpdet: BUILD_TARGET = all
pmpdet: VALGRIND_CMD =
pmpdet: executepmpdet

pmpdet-memcheck: BUILD_TARGET = debug
pmpdet-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
pmpdet-memcheck: executepmpdet

pmpdet-helgrind: BUILD_TARGET = debug
pmpdet-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
pmpdet-helgrind: executepmpdet

executepmpdet: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/pmp-det.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/pmp.config PMP.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 32 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 64 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución PMP2 ---
pmp2: BUILD_TARGET = all
pmp2: VALGRIND_CMD =
pmp2: executepmp2

pmp2-memcheck: BUILD_TARGET = debug
pmp2-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
pmp2-memcheck: executepmp2

pmp2-helgrind: BUILD_TARGET = debug
pmp2-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
pmp2-helgrind: executepmp2

executepmp2: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/pmp.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/pmp.config PMP_v2.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 16 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 32 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 64 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución PMP2-DET ---
pmpdet2: BUILD_TARGET = all
pmpdet2: VALGRIND_CMD =
pmpdet2: executepmpdet2

pmpdet2-memcheck: BUILD_TARGET = debug
pmpdet2-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
pmpdet2-memcheck: executepmpdet2

pmpdet2-helgrind: BUILD_TARGET = debug
pmpdet2-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
pmpdet2-helgrind: executepmpdet2

executepmpdet2: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/pmp-det.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/pmp.config PMP_v2.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 16 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 32 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1000.config 64 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución PHP ---
php: BUILD_TARGET = all
php: VALGRIND_CMD =
php: executephp

php-memcheck: BUILD_TARGET = debug
php-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
php-memcheck: executephp

php-helgrind: BUILD_TARGET = debug
php-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
php-helgrind: executephp

executephp: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/php.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/php.config PHP.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución PHP2 ---
php2: BUILD_TARGET = all
php2: VALGRIND_CMD =
php2: executephp2

php2-memcheck: BUILD_TARGET = debug
php2-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
php2-memcheck: executephp2

php2-helgrind: BUILD_TARGET = debug
php2-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
php2-helgrind: executephp2

executephp2: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/php.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/php.config PHP_v2.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/1500.config 16 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución ES3_1 ---
es31: BUILD_TARGET = all
es31: VALGRIND_CMD =
es31: executees31

es31-memcheck: BUILD_TARGET = debug
es31-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
es31-memcheck: executees31

es31-helgrind: BUILD_TARGET = debug
es31-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
es31-helgrind: executees31

executees31: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/ES3.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/ES3.config ES3_1.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-2 > ./output/cpu_2.log 2>&1 &
	@sleep $(ESPERA_CPUS)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-3 > ./output/cpu_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-4 > ./output/cpu_4.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución ES3_2 ---
es32: BUILD_TARGET = all
es32: VALGRIND_CMD =
es32: executees32

es32-memcheck: BUILD_TARGET = debug
es32-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
es32-memcheck: executees32

es32-helgrind: BUILD_TARGET = debug
es32-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
es32-helgrind: executees32

executees32: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/ES3.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/ES3.config ES3_2.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-2 > ./output/cpu_2.log 2>&1 &
	@sleep $(ESPERA_CPUS)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-3 > ./output/cpu_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-4 > ./output/cpu_4.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución ES3_3 ---
es33: BUILD_TARGET = all
es33: VALGRIND_CMD =
es33: executees33

es33-memcheck: BUILD_TARGET = debug
es33-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
es33-memcheck: executees33

es33-helgrind: BUILD_TARGET = debug
es33-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
es33-helgrind: executees33

executees33: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/ES3.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/ES3.config ES3_3.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-2 > ./output/cpu_2.log 2>&1 &
	@sleep $(ESPERA_CPUS)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-3 > ./output/cpu_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-4 > ./output/cpu_4.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
	@echo "Revisá la carpeta ./output/ para ver los reportes de Valgrind de cada módulo."
	@echo "Usa 'make kill' para detener todo."

# --- Modos de ejecución ES3_4 ---
es34: BUILD_TARGET = all
es34: VALGRIND_CMD =
es34: executees34

es34-memcheck: BUILD_TARGET = debug
es34-memcheck: VALGRIND_CMD = $(VALGRIND_MEMCHECK)
es34-memcheck: executees34

es34-helgrind: BUILD_TARGET = debug
es34-helgrind: VALGRIND_CMD = $(VALGRIND_HELGRIND)
es34-helgrind: executees34

executees34: logs $(BUILD_TARGET)
	@mkdir -p ./output
	@echo "Lanzando sistema base con: [$(VALGRIND_CMD)] ..."
	
	$(VALGRIND_CMD) ./kernel_memory/bin/kernel_memory ./kernel_memory/configs/ES3.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/configs/ES3.config ES3_4.prc > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./memory_stick/bin/memory_stick ./memory_stick/configs/150.config 2048 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDIN < ./tests/pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-2 > ./output/cpu_2.log 2>&1 &
	@sleep $(ESPERA_CPUS)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-3 > ./output/cpu_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	$(VALGRIND_CMD) ./cpu/bin/cpu ./cpu/cpu.config CPU-4 > ./output/cpu_4.log 2>&1 &
	
	@echo "Sistema base lanzado con éxito. La terminal está libre."
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
