#!/usr/bin/env python3

import re
import sys
from collections import Counter


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: deduplicate_docstr.py <docstr.h>", file=sys.stderr)
        return 1

    filename = sys.argv[1]
    with open(filename, "r", encoding="utf-8") as f:
        lines = f.readlines()

    pattern = re.compile(r"(static const char \*)(__doc_[A-Za-z0-9_]+)(\b)")
    names = []
    for line in lines:
        match = pattern.match(line)
        if match:
            names.append(match.group(2))

    all_names = set(names)
    seen = Counter()
    replacements = 0

    for i, line in enumerate(lines):
        match = pattern.match(line)
        if not match:
            continue

        name = match.group(2)
        seen[name] += 1
        if seen[name] == 1:
            continue

        suffix = 2
        while True:
            replacement = f"{name}_{suffix}"
            if replacement not in all_names:
                break
            suffix += 1

        lines[i] = pattern.sub(rf"\1{replacement}\3", line, count=1)
        all_names.add(replacement)
        replacements += 1

    if replacements:
        with open(filename, "w", encoding="utf-8") as f:
            f.writelines(lines)
        print(f"Renamed {replacements} duplicate docstring symbol(s).")

    return 0


if __name__ == "__main__":
    sys.exit(main())
