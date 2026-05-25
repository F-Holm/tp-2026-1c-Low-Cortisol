MODULES = cpu io kernel_memory kernel_scheduler memory_stick swap utils
SLEEP_TIME = 0.1

.PHONY: all debug release test clean logs format run kill $(MODULES)

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

run: all
	@mkdir -p ./output
	@echo "Lanzando sistema..."
	./kernel_memory/bin/kernel_memory ./kernel_memory/kernel_memory.config > ./output/kernel_memory.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernel_scheduler.config proceso_inicial.asm > ./output/kernel_scheduler.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 1000 > ./output/memory_stick_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 2000 > ./output/memory_stick_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 3000 > ./output/memory_stick_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./swap/bin/swap ./swap/swap.config > ./output/swap.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./io/bin/io ./io/io.config SLEEP > ./output/io_sleep.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./io/bin/io ./io/io.config STDIN < ./pseudocodigo/entradas_io_stdin.txt > ./output/io_stdin.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./io/bin/io ./io/io.config STDOUT > ./output/io_stdout.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./cpu/bin/cpu ./cpu/cpu.config CPU-2 > ./output/cpu_2.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./cpu/bin/cpu ./cpu/cpu.config CPU-3 > ./output/cpu_3.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 4000 > ./output/memory_stick_4.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 5000 > ./output/memory_stick_5.log 2>&1 &
	@sleep $(SLEEP_TIME)
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 6000 > ./output/memory_stick_6.log 2>&1 &
	@echo "Sistema lanzado con éxito. La terminal está libre."
	@echo "Usa 'make kill' para detener todo (si no falló antes)."

kill:
	@echo "Cerrando el sistema..."
	-pkill -f kernel_memory
	-pkill -f kernel_scheduler

$(MODULES):
	$(MAKE) -C $@
