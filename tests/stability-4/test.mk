INITIAL_PROCESS := STABILITY_4.prc
STICK_SIZES     := 2048 2048 2048 2048
CPU_COUNT       := 4
CPU_STAGGER     := 2

# Never reaches SR_NO_PROCESSES on its own: the initial process loops on
# itself (SET PC 0) and keeps spawning children forever. run_e2e.py lets it
# run for TIMEOUT seconds, then disconnects memory_stick_1 to force a BSOD,
# and gives it TIMEOUT_BSOD more seconds to shut down cleanly.
TERMINATION := infinite
TIMEOUT     := 3600
TIMEOUT_BSOD := 900
