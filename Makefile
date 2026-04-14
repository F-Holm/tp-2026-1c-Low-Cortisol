MODULES = cpu io kernel_memory kernel_scheduler memory_stick swap utils
RUN_LIST = kernel_memory kernel_scheduler memory_stick memory_stick memory_stick swap io io io cpu cpu cpu

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

format:
	find . -iname "*.c" -o -iname "*.h" | grep -v "tests/" | xargs clang-format -i --style=file

run:
	@for mod in $(RUN_LIST); do \
		if [ -f ./$$mod/bin/$$mod ]; then \
			echo "Iniciando $$mod..."; \
			./$$mod/bin/$$mod > /dev/null 2>&1 & \
			sleep 1; \
		else \
			echo "Error: Binario ./$$mod/bin/$$mod no encontrado."; \
		fi \
	done

kill:
	@pkill -f "./(cpu|io|kernel_memory|kernel_scheduler|memory_stick|swap)/bin/" || echo "No había procesos corriendo."

$(MODULES):
	$(MAKE) -C $@
