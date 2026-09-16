INITIAL_PROCESS := PRIORITY_INHERITANCE_V2.prc
STICK_SIZES     := 16 16
CPU_COUNT       := 1
CPU_STAGGER     :=

# Never reaches SR_NO_PROCESSES on its own -- by design, this workload
# includes a process whose EXIT is unreachable (kept looping on purpose to
# soak-test the system). run_e2e.py lets it run for TIMEOUT seconds, then
# disconnects memory_stick_1 to force a BSOD, and gives it TIMEOUT_BSOD more
# seconds to shut down cleanly.
TERMINATION := infinite
TIMEOUT     := 3600
TIMEOUT_BSOD := 900
