#!/usr/bin/env python3
"""Runs the end-to-end scenarios under tests/ and reports pass/fail.

Each scenario directory (tests/<scenario>/, everything except pseudocode/)
declares its parameters in test.mk (TERMINATION, TIMEOUT, TIMEOUT_BSOD) and
its pass conditions in checks.txt (one line per condition: an operator, a
count, and a regex matched against every log line of the run):

    ==1  Shutting down: Processes finished successfully
    ==0  Shutting down: BSOD: Corruption of memory detected
    >=1  Change of priority:

TERMINATION is either:
  - finite:   the scenario is expected to shut down on its own (every
              process finishes) within TIMEOUT seconds. If it doesn't, that
              is treated as a hang (deadlock or similar) and the scenario
              fails outright -- nothing is forced.
  - infinite: the workload never finishes on its own by design. Running for
              TIMEOUT seconds without shutting down is expected; the runner
              then kills memory_stick_1 to force a BSOD (Kernel Memory
              reports corrupted memory, Kernel Scheduler shuts down), and
              gives it TIMEOUT_BSOD more seconds to do so cleanly. Failing
              to shut down even after that is a real hang.

Scenarios run concurrently (bounded by --jobs); each uses its own port pair
(see tests/README.md) and its own output/<scenario>/ directory for logs and
PID files, and is torn down by those PIDs rather than by `make kill`, so
parallel scenarios never affect each other.

Usage:
    python3 tests/run_e2e.py                      # build once, run every scenario
    python3 tests/run_e2e.py --scenario base mem-best
    python3 tests/run_e2e.py --jobs 1              # sequential
    python3 tests/run_e2e.py --mode memcheck       # every process under Valgrind
    python3 tests/run_e2e.py --list                # show parsed parameters, run nothing
    python3 tests/run_e2e.py --skip-build          # assume `make all` already ran
"""

from __future__ import annotations

import argparse
import os
import re
import signal
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
TESTS_DIR = REPO_ROOT / "tests"
POLL_INTERVAL = 0.5
TEARDOWN_GRACE = 2.0

MAKE_VAR_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:?=\s*(.*?)\s*$")
CHECK_LINE_RE = re.compile(r"^\s*(==|<=|>=|<|>)\s*(\d+)\s+(.+?)\s*$")
SHUTDOWN_RE = re.compile(r"Shutting down: (.+)")
ERROR_SUMMARY_RE = re.compile(r"ERROR SUMMARY:\s*(\d+)\s+errors")


@dataclass
class Check:
    op: str
    expected: int
    pattern: str

    def evaluate(self, count: int) -> bool:
        return {
            "==": count == self.expected,
            "<": count < self.expected,
            ">": count > self.expected,
            "<=": count <= self.expected,
            ">=": count >= self.expected,
        }[self.op]

    def __str__(self) -> str:
        return f"{self.op}{self.expected} /{self.pattern}/"


@dataclass
class Scenario:
    name: str
    termination: str
    timeout: float
    timeout_bsod: float
    checks: list[Check]

    @property
    def output_dir(self) -> Path:
        return REPO_ROOT / "output" / self.name


@dataclass
class Result:
    scenario: str
    passed: bool
    reasons: list[str] = field(default_factory=list)
    duration: float = 0.0


def parse_make_vars(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in path.read_text().splitlines():
        line = raw_line.split("#", 1)[0]
        m = MAKE_VAR_RE.match(line)
        if m:
            values[m.group(1)] = m.group(2)
    return values


def load_scenario(name: str) -> Scenario:
    directory = TESTS_DIR / name
    variables = parse_make_vars(directory / "test.mk")
    termination = variables.get("TERMINATION", "finite")
    timeout = float(variables.get("TIMEOUT", "60"))
    timeout_bsod = float(variables.get("TIMEOUT_BSOD", "30"))

    checks: list[Check] = []
    checks_file = directory / "checks.txt"
    if checks_file.exists():
        for raw_line in checks_file.read_text().splitlines():
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            m = CHECK_LINE_RE.match(line)
            if not m:
                raise ValueError(f"{checks_file}: malformed line: {raw_line!r}")
            checks.append(Check(m.group(1), int(m.group(2)), m.group(3)))

    return Scenario(name, termination, timeout, timeout_bsod, checks)


def discover_scenarios() -> list[str]:
    return sorted(
        p.name
        for p in TESTS_DIR.iterdir()
        if p.is_dir() and p.name != "pseudocode" and (p / "test.mk").exists()
    )


def read_pid(path: Path) -> int | None:
    try:
        return int(path.read_text().strip())
    except (FileNotFoundError, ValueError):
        return None


def is_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def read_log_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return ""


def kernel_scheduler_shutdown_reason(scenario: Scenario) -> str | None:
    text = read_log_text(scenario.output_dir / "kernel_scheduler.log")
    m = SHUTDOWN_RE.search(text)
    return m.group(1) if m else None


def find_unexpected_crash(scenario: Scenario, ignore: frozenset[str] = frozenset()) -> str | None:
    """Checks every recorded process except kernel_scheduler (which is
    supposed to exit once it reaches a shutdown reason) and anything in
    `ignore` (e.g. a stick we killed on purpose). Meant to be called only
    while still waiting for that shutdown reason -- once it's been reached,
    every other module cascading-shutting-down on its own is the expected,
    correct ending, not a crash."""
    for pid_file in sorted(scenario.output_dir.glob("*.pid")):
        name = pid_file.stem
        if name == "kernel_scheduler" or name in ignore:
            continue
        pid = read_pid(pid_file)
        if pid is not None and not is_alive(pid):
            return f"{name} is not running anymore (possible crash)"
    return None


def wait_for_shutdown(scenario: Scenario, timeout: float,
                      ignore_crash: frozenset[str] = frozenset(),
                      stop_when_ready: bool = False) -> tuple[str | None, str | None, bool]:
    """Polls kernel_scheduler's log for a 'Shutting down: ...' line, up to
    `timeout` seconds, while also watching that nothing else dies before it
    gets there. If `stop_when_ready`, also returns early (ready=True) once
    checks_ready_early() is satisfied -- meant for an `infinite` scenario's
    soak, so it doesn't sit out the rest of a (deliberately generous, for
    Valgrind) TIMEOUT once the behaviour it exists to demonstrate already
    happened. Returns (reason, crash, ready); at most one of reason/crash is
    set, and ready is only ever True when neither is."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        reason = kernel_scheduler_shutdown_reason(scenario)
        if reason is not None:
            return reason, None, False
        crash = find_unexpected_crash(scenario, ignore_crash)
        if crash is not None:
            # kernel_scheduler's own "Shutting down: ..." line can lag behind
            # a peer noticing the cascading disconnect and exiting -- its
            # stdout isn't flushed as promptly as a socket close propagates.
            # Give it a couple of seconds to catch up before trusting the
            # crash reading.
            grace_deadline = time.monotonic() + 2.0
            while time.monotonic() < grace_deadline:
                reason = kernel_scheduler_shutdown_reason(scenario)
                if reason is not None:
                    return reason, None, False
                time.sleep(0.2)
            return None, crash, False
        if stop_when_ready and checks_ready_early(scenario):
            return None, None, True
        time.sleep(POLL_INTERVAL)
    return kernel_scheduler_shutdown_reason(scenario), None, False


def force_disconnect_first_stick(scenario: Scenario) -> bool:
    """Kills memory_stick_1 outright (no graceful shutdown) to simulate it
    being unplugged, so Kernel Memory reports corrupted memory and Kernel
    Scheduler shuts down with the BSOD reason. Returns whether a PID was
    found to kill."""
    pid = read_pid(scenario.output_dir / "memory_stick_1.pid")
    if pid is None or not is_alive(pid):
        return False
    os.kill(pid, signal.SIGKILL)
    return True


def teardown(scenario: Scenario) -> None:
    pids = []
    for pid_file in scenario.output_dir.glob("*.pid"):
        pid = read_pid(pid_file)
        if pid is not None:
            pids.append(pid)

    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

    deadline = time.monotonic() + TEARDOWN_GRACE
    while time.monotonic() < deadline and any(is_alive(p) for p in pids):
        time.sleep(0.2)

    for pid in pids:
        if is_alive(pid):
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


def gather_log_lines(scenario: Scenario) -> list[str]:
    lines: list[str] = []
    for log_file in sorted(scenario.output_dir.glob("*.log")):
        lines.extend(read_log_text(log_file).splitlines())
    return lines


def evaluate_checks(scenario: Scenario) -> list[str]:
    lines = gather_log_lines(scenario)
    failures = []
    for check in scenario.checks:
        pattern = re.compile(check.pattern)
        count = sum(1 for line in lines if pattern.search(line))
        if not check.evaluate(count):
            failures.append(f"check failed: {check} -> got {count}")
    return failures


def checks_ready_early(scenario: Scenario) -> bool:
    """For `infinite` scenarios: the `>=`/`>` checks describe the behaviour
    the scenario exists to demonstrate (a count of at least N). Once the
    current logs already satisfy every one of them, there's no reason to
    keep soaking for the rest of TIMEOUT -- the interesting part already
    happened. `==`/`<=`/`<` checks aren't used for this: they either don't
    make sense as a "done" signal (an upper bound could still be violated
    later) or are only meaningful once the run has actually ended."""
    early_checks = [c for c in scenario.checks if c.op in (">=", ">")]
    if not early_checks:
        return False
    lines = gather_log_lines(scenario)
    for check in early_checks:
        pattern = re.compile(check.pattern)
        count = sum(1 for line in lines if pattern.search(line))
        if not check.evaluate(count):
            return False
    return True


def evaluate_valgrind(scenario: Scenario) -> list[str]:
    failures = []
    for log_file in sorted(scenario.output_dir.glob("*.log")):
        text = read_log_text(log_file)
        m = ERROR_SUMMARY_RE.search(text)
        if m and int(m.group(1)) != 0:
            failures.append(f"{log_file.name}: {m.group(0)}")
    return failures


def run_scenario(scenario: Scenario, mode: str | None) -> Result:
    started = time.monotonic()
    reasons: list[str] = []

    with _print_lock:
        print(f"-> starting {scenario.name} (timeout={scenario.timeout:.0f}s)", flush=True)

    make_cmd = ["make", f"{scenario.name}-run"]
    if mode:
        make_cmd.append(f"MODE={mode}")
    try:
        subprocess.run(make_cmd, cwd=REPO_ROOT, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as exc:
        teardown(scenario)
        return Result(scenario.name, passed=False,
                      reasons=[f"'make {scenario.name}-run' failed: {exc.stderr.strip()}"],
                      duration=time.monotonic() - started)

    is_infinite = scenario.termination == "infinite"
    reason, crash, ready = wait_for_shutdown(scenario, scenario.timeout, stop_when_ready=is_infinite)
    if ready:
        with _print_lock:
            print(f"   {scenario.name}: checks already satisfied, not waiting out the rest of the soak",
                  flush=True)

    if crash is not None:
        reasons.append(crash)
    elif reason is None:
        # Either it genuinely timed out, or (only possible when infinite)
        # `ready` fired early because checks_ready_early() was already
        # satisfied -- either way, an infinite scenario proceeds to forcing
        # the disconnect now instead of waiting out the rest of TIMEOUT.
        if not is_infinite:
            reasons.append(
                f"did not shut down within {scenario.timeout}s "
                "(assumed deadlock or similar)"
            )
        else:
            if not force_disconnect_first_stick(scenario):
                reasons.append("infinite scenario, but memory_stick_1.pid was missing")
            else:
                reason, crash, _ = wait_for_shutdown(scenario, scenario.timeout_bsod,
                                                     ignore_crash=frozenset({"memory_stick_1"}))
                if crash is not None:
                    reasons.append(crash)
                elif reason is None:
                    reasons.append(
                        f"did not shut down within {scenario.timeout_bsod}s "
                        "of forcing the stick disconnect (hung even under BSOD)"
                    )

    if not reasons:
        reasons.extend(evaluate_checks(scenario))
        if mode:
            reasons.extend(evaluate_valgrind(scenario))

    teardown(scenario)

    result = Result(scenario.name, passed=not reasons, reasons=reasons,
                    duration=time.monotonic() - started)
    print_result(result)
    return result


_print_lock = threading.Lock()


def print_result(result: Result) -> None:
    with _print_lock:
        status = "PASS" if result.passed else "FAIL"
        print(f"[{status}] {result.scenario} ({result.duration:.1f}s)", flush=True)
        for reason in result.reasons:
            print(f"         - {reason}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--scenario", nargs="*", help="run only these scenarios")
    parser.add_argument("--jobs", type=int, default=4, help="max scenarios in parallel (default: 4)")
    parser.add_argument("--mode", choices=["memcheck", "helgrind"], help="run under Valgrind")
    parser.add_argument("--skip-build", action="store_true", help="assume `make all` already ran")
    parser.add_argument("--list", action="store_true", help="print parsed parameters and exit")
    args = parser.parse_args()

    names = args.scenario if args.scenario else discover_scenarios()
    scenarios = [load_scenario(name) for name in names]

    if args.list:
        for s in scenarios:
            print(f"{s.name}: {s.termination}, timeout={s.timeout}s"
                  + (f", timeout_bsod={s.timeout_bsod}s" if s.termination == "infinite" else ""))
            for c in s.checks:
                print(f"    {c}")
        return 0

    if not args.skip_build:
        print("Building (make all)...")
        subprocess.run(["make", "all"], cwd=REPO_ROOT, check=True)

    (REPO_ROOT / "output").mkdir(exist_ok=True)

    print(f"Running {len(scenarios)} scenario(s), up to {args.jobs} at a time"
          + (f" [MODE={args.mode}]" if args.mode else "") + "...")

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda s: run_scenario(s, args.mode), scenarios))

    failed = [r for r in results if not r.passed]
    print(f"\n{len(results) - len(failed)}/{len(results)} scenarios passed.")
    return len(failed)


if __name__ == "__main__":
    sys.exit(main())
