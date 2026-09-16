INITIAL_PROCESS := MEM_PRE_0.prc
STICK_SIZES     := 256
CPU_COUNT       := 1
CPU_STAGGER     :=

# Expected to reach SR_NO_PROCESSES (every process finishes) on its own
# within TIMEOUT seconds; if it doesn't, run_e2e.py treats it as a hang
# (deadlock or similar) and fails the scenario without forcing anything.
TERMINATION := finite
TIMEOUT     := 3600
