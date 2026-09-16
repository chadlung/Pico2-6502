# PC builds: the simulator, Fake6502's CPU test suites and the disassembler test.
#
#   make              build sim6502
#   make test         run the CPU test suites against src/fake6502
#   make test-disasm  check the disassembler against 64tass (needs 64tass)

CC     ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra

SRC  := ../src
CORE := $(SRC)/fake6502
SIM_SOURCES := sim_main.c $(SRC)/bus.c $(SRC)/disasm.c $(SRC)/lcd.c $(SRC)/monitor.c $(CORE)/fake6502.c
HEADERS := $(wildcard $(SRC)/*.h) $(CORE)/fake6502.h

sim6502: $(SIM_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -I$(SRC) -I$(CORE) -o $@ $(SIM_SOURCES)

# The test programs and harness come from the upstream Fake6502 repository.
fake6502-upstream:
	git clone --depth 1 https://github.com/ivop/fake6502.git $@

# Copy the harness so it compiles against this project's fake6502.h.
cputest: fake6502-upstream $(CORE)/fake6502.c $(CORE)/fake6502.h
	cp fake6502-upstream/test.c cputest.c
	$(CC) -O2 -I$(CORE) -o $@ cputest.c $(CORE)/fake6502.c

test: cputest
	cd fake6502-upstream && ../cputest | tee ../cputest.log
	@! grep -q FAIL cputest.log && echo "All CPU tests passed."

disasm_dump: disasm_dump.c $(SRC)/disasm.c $(SRC)/disasm.h
	$(CC) $(CFLAGS) -I$(SRC) -o $@ disasm_dump.c $(SRC)/disasm.c

test-disasm: disasm_dump
	python3 test_disasm.py

clean:
	rm -f sim6502 cputest cputest.c cputest.log disasm_dump

distclean: clean
	rm -rf fake6502-upstream

.PHONY: test test-disasm clean distclean
