#!/usr/bin/env python3
"""Generate a large, reproducible text file for benchmarking.

    python tools/gen_text.py 200 big.txt      # ~200 MiB, one "line" = 5..25 words
"""
import random
import sys

WORDS = ("lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor "
         "incididunt ut labore et dolore magna aliqua enim ad minim veniam quis nostrud "
         "exercitation ullamco laboris nisi aliquip ex ea commodo consequat").split()


def main() -> None:
    size_mib = float(sys.argv[1]) if len(sys.argv) > 1 else 100
    path = sys.argv[2] if len(sys.argv) > 2 else "big.txt"
    target = int(size_mib * 1024 * 1024)
    rng = random.Random(42)  # fixed seed creates identical file on every machine
    written = 0
    with open(path, "w", encoding="ascii", newline="\n") as f:
        while written < target:
            line = " ".join(rng.choices(WORDS, k=rng.randint(5, 25))) + "\n"
            f.write(line)
            written += len(line)
    print(f"wrote {written / 1024 / 1024:.1f} MiB to {path}")


if __name__ == "__main__":
    main()
