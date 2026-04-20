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
	-rm -f ./*.log ./*/*.log
	@echo "Logs eliminados correctamente."

format:
	find . -iname "*.c" -o -iname "*.h" | grep -v "tests/" | xargs clang-format -i --style=file

run: all
	@echo "Lanzando sistema..."
	./kernel_memory/bin/kernel_memory ./kernel_memory/kernel_memory.config > /dev/null 2>&1 &
	@sleep 0.1
	./kernel_scheduler/bin/kernel_scheduler ./kernel_scheduler/kernel_scheduler.config ./kernel_scheduler/proceso_inicial > /dev/null 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 1000 > /dev/null 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 2000 > /dev/null 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 3000 > /dev/null 2>&1 &
	@sleep 0.1
	./swap/bin/swap ./swap/swap.config > /dev/null 2>&1 &
	@sleep 0.1
	./io/bin/io ./io/io.config SLEEP > /dev/null 2>&1 &
	@sleep 0.1
	./io/bin/io ./io/io.config STDIN > /dev/null 2>&1 &
	@sleep 0.1
	./io/bin/io ./io/io.config STDOUT > /dev/null 2>&1 &
	@sleep 0.1
	./cpu/bin/cpu ./cpu/cpu.config CPU-1 > /dev/null 2>&1 &
	@sleep 0.1
	./cpu/bin/cpu ./cpu/cpu.config CPU-2 > /dev/null 2>&1 &
	@sleep 0.1
	./cpu/bin/cpu ./cpu/cpu.config CPU-3 > /dev/null 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 4000 > /dev/null 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 5000 > /dev/null 2>&1 &
	@sleep 0.1
	./memory_stick/bin/memory_stick ./memory_stick/memory_stick.config 6000 > /dev/null 2>&1 &
	@echo "Sistema lanzado con éxito. La terminal está libre."
	@echo "Usa 'make kill' para detener todo (si no falló antes)."

kill:
	@echo "Cerrando el sistema..."
	-pkill -f kernel_memory

$(MODULES):
	$(MAKE) -C $@
