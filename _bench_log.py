"""Minimal logging shim for the benchmark utility (stripped from pbdr.log)."""

import sys


def info(msg, *args):
    print(msg, *args, flush=True)


def warn(msg, *args):
    print("[WARN]", msg, *args, flush=True, file=sys.stderr)


class ProgressBar:
    def __init__(self, label, total, disabled=False):
        self.label = label
        self.total = total
        self.disabled = disabled
        self.count = 0

    def __enter__(self):
        if not self.disabled:
            print(f"  {self.label} (0/{self.total})", end="", flush=True)
        return self

    def __exit__(self, *args):
        if not self.disabled:
            print(" done.", flush=True)

    def step(self):
        self.count += 1
        if not self.disabled:
            print(f"\r  {self.label} ({self.count}/{self.total})", end="", flush=True)
