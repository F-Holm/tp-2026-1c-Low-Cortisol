INITIAL_PROCESS := initial_process.prc
STICK_SIZES     := 1000 2000 3000 4000 5000 6000
CPU_COUNT       := 3
CPU_STAGGER     :=

# Expected to reach SR_NO_PROCESSES (every process finishes) on its own
# within TIMEOUT seconds; if it doesn't, run_e2e.py treats it as a hang
# (deadlock or similar) and fails the scenario without forcing anything.
TERMINATION := finite
TIMEOUT     := 3600
