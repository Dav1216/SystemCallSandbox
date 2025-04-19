# Compiler and flags
CC = gcc
CFLAGS = -fPIC -Wno-deprecated

# Policy file
POLICY_FILE = "policy_file.txt"

# Executables and sources
DUMMY_EXEC = dummy
POLICY_MAKER_EXEC = policy_maker
ATTACKER_EXEC = attacker
SANDBOX_LIBRARY = sandbox.so

DUMMY_SRC = dummy.c
POLICY_MAKER_SRC = policy_maker.c
ATTACKER_SRC = attacker.c
SANDBOX_SRC = sandbox.c

# Phony targets
.PHONY: all run trace clean

# Default build target
all: $(POLICY_MAKER_EXEC) $(DUMMY_EXEC) $(SANDBOX_LIBRARY) $(ATTACKER_EXEC)

# Build policy maker (tracer)
$(POLICY_MAKER_EXEC): $(POLICY_MAKER_SRC)
	$(CC) -o $@ $<

# Build dummy
$(DUMMY_EXEC): $(DUMMY_SRC)
	$(CC) -o $@ $<

# Trace a target program (default: dummy)
EXE ?= ./dummy
RUNS ?= 5
trace: $(POLICY_MAKER_EXEC) $(DUMMY_EXEC)
	./$(POLICY_MAKER_EXEC) $(EXE) --runs $(RUNS)

# Build sandbox shared library
$(SANDBOX_LIBRARY): $(SANDBOX_SRC)
	$(CC) $(CFLAGS) -DPOLICY_FILE=\"$(POLICY_FILE)\" -shared -o $@ $<

# Build attacker
$(ATTACKER_EXEC): $(ATTACKER_SRC)
	$(CC) -o $@ $<

# Run attacker with sandbox preloaded
run: $(SANDBOX_LIBRARY) $(ATTACKER_EXEC)
	LD_PRELOAD=./$(SANDBOX_LIBRARY) ./$(ATTACKER_EXEC)

# Clean up build artifacts
clean:
	rm -f $(SANDBOX_LIBRARY) $(ATTACKER_EXEC) $(DUMMY_EXEC) $(POLICY_MAKER_EXEC)
