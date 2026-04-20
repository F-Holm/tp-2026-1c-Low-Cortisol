MODULES = cpu io kernel_memory kernel_scheduler memory_stick swap utils

.PHONY: all debug release test clean format $(MODULES)

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
	./kernel_memory/bin/kernel_memory ./kernel_memory/kernel_memory.config > ./output/kernel_memory.txt 2>&1 &
	@sleep 0.1
	./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernel_scheduler.config ./kernel_scheduler/proceso_inicial > ./output/kernel_scheduler.txt 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 1000 > ./output/memory_stick_1.txt 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 2000 > ./output/memory_stick_2.txt 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 3000 > ./output/memory_stick_3.txt 2>&1 &
	@sleep 0.1
	./swap/bin/swap ./swap/swap.config > ./output/swap.txt 2>&1 &
	@sleep 0.1
	./io/bin/io ./io/io.config SLEEP > ./output/io_SLEEP.txt 2>&1 &
	@sleep 0.1
	./io/bin/io ./io/io.config STDIN > ./output/io_STDIN.txt 2>&1 &
	@sleep 0.1
	./io/bin/io ./io/io.config STDOUT > ./output/io_STDOUT.txt 2>&1 &
	@sleep 0.1
	./cpu/bin/cpu ./cpu/cpu.config CPU-1 > ./output/cpu_1.txt 2>&1 &
	@sleep 0.1
	./cpu/bin/cpu ./cpu/cpu.config CPU-2 > ./output/cpu_2.txt 2>&1 &
	@sleep 0.1
	./cpu/bin/cpu ./cpu/cpu.config CPU-3 > ./output/cpu_3.txt 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 4000 > ./output/memory_stick_4.txt 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 5000 > ./output/memory_stick_5.txt 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 6000 > ./output/memory_stick_6.txt 2>&1 &
	@echo "Sistema lanzado con éxito. La terminal está libre."
	@echo "Usa 'make kill' para detener todo (si no falló antes)."

kill:
	@echo "Cerrando el sistema..."
	-pkill -f kernel_memory

$(MODULES):
	$(MAKE) -C $@
