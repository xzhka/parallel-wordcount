<h1 align="center">Parallel Wordcount</h1>
<p align="center">
  <b>Sequential vs. multithreaded word counting, with a built-in correctness check and measured speedup</b><br>
  C++17 · std::thread · CMake · no mutex needed · Windows / Linux / macOS
</p>
<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake" alt="CMake 3.16+">
  <img src="https://img.shields.io/badge/Threads-std%3A%3Athread-orange" alt="std::thread">
  <img src="https://img.shields.io/badge/Speedup-15.5x%20on%2024%20threads-green" alt="15.5x speedup on 24 threads">
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey" alt="Platforms">
  <img src="https://img.shields.io/badge/License-MIT-yellow" alt="MIT license">
</p>

Counting the words of a large text file with `std::thread` in C++17, and measuring what the extra threads actually buy.

**Result:** 16.1 million words in a 100 MiB file are counted in **167 ms sequentially and 10.8 ms on 24 threads (15.5x)**, and the parallel count matches the sequential one on every run.

This started as a multithreading study some years ago. I revisited it, fixed its concurrency bugs, and re-measured it on current hardware.

## What is in the repo

| Folder | What it is |
|---|---|
| `word_count_solo/` | Sequential baseline: read lines, split into chunks, count words. |
| `word_count_parallel/` | The same job on N worker threads. Checks its result against the sequential count and reports the speedup. |
| `function_ratio/` | Evaluates `f(a)` and `f(b)` on two threads and prints `f(a)/f(b)`. A tiny demo of the start, join, read pattern. |
| `data/` | A small sample text (`lorem1000.txt`, 1000 words). |
| `tools/gen_text.py` | Generates a large, reproducible test file (fixed random seed, so every machine gets the same file). |

## Build

Needs a C++17 compiler and CMake 3.16 or newer. CI builds on Windows, Linux and macOS.

```bash
cmake -S . -B build
cmake --build build --config Release
```

In Visual Studio you can also use *File > Open > Folder* on this directory. Always time the Release build; a Debug build tells you nothing.

## Run

```bash
python tools/gen_text.py 100 big.txt   # 100 MiB test file
```

Windows (Visual Studio generator puts the executables in `build\Release\`):

```powershell
.\build\Release\word_count_parallel.exe
```

Linux / macOS:

```bash
./build/word_count_parallel
```

The program asks for a file name, a chunk size (lines per chunk) and a thread count (`0` = use all hardware threads). The file name is relative to the folder you start the program from, so run it from the repo root:

```text
Enter file name: big.txt
Enter a chunk size: 100
Enter number of threads (0 = auto): 0
Words (parallel):   16134108
Words (sequential): 16134108
Results match:      yes
Lines / chunks:     1075124 / 10752
Threads used:       24
Chunking time:      96.3594 msec
Sequential count:   166.975 msec (best of 3)
Parallel count:     10.771 msec (best of 3)
Speedup:            15.5023x
```

`word_count_parallel` exits with a non-zero code if the parallel result differs from the sequential one. `word_count_solo` asks for the same file name and chunk size and prints the word count and timings. `function_ratio` takes no input (`a = 75.6` and `b = 50.96` are set in `main`):

```text
f(a) = 4.92936121722e+108
f(b) = 2.64282473525e+64
f(a) / f(b) = 1.86518657536e+44
Matches sequential: yes
Execution time 447 usec
```

It runs `f(a)` and `f(b)` on two threads, then repeats both calls on one thread and prints `Matches sequential: yes` only if the results are identical. The 447 µs includes starting both threads, which is why this program shows the pattern and not a speedup.

## How the parallel version works

1. The lines of the file are grouped into chunks of `chunkSize` lines (`ceil(lines / chunkSize)` chunks).
2. The chunks are shared among `N` workers. Everyone gets `chunks / N` chunks and the remainder goes one each to the first workers (23 chunks, 4 workers: `[0,6) [6,12) [12,18) [18,23)`). `N` is clamped to the number of chunks.
3. Worker `i` counts its range and writes **only** `partial[i]`.
4. After `join()` the main thread sums the slots.

**No mutex is needed**, because nothing is both shared and written: the chunks are read-only while the workers run, each result slot has exactly one writer, and the main thread reads the slots only after `join()`.

## Results

**Machine:** AMD Ryzen 9 7900X (12 cores / 24 threads), 31 GB RAM, Windows, MSVC 19.44 (Visual Studio 2022), CMake 4.3.1, Release build.
**Input:** `python tools/gen_text.py 100 big.txt` (100 MiB, 1,075,124 lines, 16,134,108 words), chunk size 100 (10,752 chunks).

| threads | sequential (ms) | parallel (ms) | speedup | speedup incl. chunking |
|--------:|----------------:|--------------:|--------:|-----------------------:|
|       1 |           160.2 |         162.6 |   0.99x |                  0.99x |
|       2 |           166.7 |          80.8 |   2.06x |                  1.48x |
|       4 |           166.7 |          42.3 |   3.94x |                  1.90x |
|       8 |           162.1 |          21.4 |   7.58x |                  2.20x |
|      12 |           160.3 |          14.6 |  10.99x |                  2.34x |
|      24 |           167.0 |          10.8 |  15.50x |                  2.46x |

Each row is one run of the program, and each time is the best of 3 repeats inside that run. The sequential baseline is re-measured in every run, which is why it moves between 160 and 167 ms.

What the numbers show:

- **Near-linear scaling up to the physical core count.** 4 threads give 3.94x and 8 give 7.58x. At 12 threads (one per core) the speedup is 10.99x, about 92% efficiency.
- **Hyper-threading helps, but less.** Going from 12 to 24 threads on the same 12 cores adds about 41% (10.99x to 15.50x). That is why 24 threads can beat a 12x "ceiling".
- **The parallel machinery is cheap.** One worker thread costs about 1.5% against the plain loop (0.99x).
- **2.06x on 2 threads is noise, not a real superlinear effect.** The sequential baseline itself varies by about 4% between runs.
- **Small inputs get slower.** On `data/lorem1000.txt` (19 lines, chunk size 10, so 2 chunks) 4 requested threads become 2, and the parallel count takes 0.0895 ms against 0.0064 ms sequentially (0.07x). Starting threads costs more than the work.
- **Only the counting step is parallel.** The last column includes the chunking step (~96 ms, copying the lines into chunks on one thread), and the whole program tops out at about 2.5x. Reading the file is not timed at all.

Run-to-run variation on a desktop is large, and this is one machine, so treat the table as a measurement, not a benchmark suite.

## Testing

There is no test framework; correctness is checked by running the programs:

- the word count was compared with `wc -w` for chunk sizes from 1 to larger than the number of lines, thread counts from 1 to 1000 (more than the number of chunks), and edge-case files (empty, no trailing newline, only blank lines);
- `word_count_parallel` compares itself with the sequential count on every run;
- the threaded programs were run under ThreadSanitizer (`-fsanitize=thread`) with no reports.

## What went wrong in the first version

The first threaded attempt used one thread to build the chunks and another to count them, both guarded by the same mutex. That had three problems:

1. **A mutex protects data, it doesn't order threads.** Nothing guaranteed the building thread ran first. When I delayed it by 100 ms the program printed `0`. It usually "worked" only because of how the threads happened to be scheduled.
2. **It was not parallel.** Holding the lock for the whole body made the two threads run one after another.
3. **The work was split by step, not by data.** Different chunks are independent, so the counting itself is what should be divided.

Other bugs fixed along the way: a loop bound that used `chunkSize` instead of the number of chunks (wrong totals and a crash), `chunkSize == 0` looping forever, an unvalidated fixed-size `char` buffer for the file name, and `function_ratio` printing results before the threads had computed them.

## Limitations and next steps

- A "word" is a run of non-whitespace characters. Whitespace means space, `\t`, `\n`, `\v`, `\f`, `\r`; bytes above 127 count as word characters, so there is no Unicode-aware splitting.
- The whole file is loaded into memory and the lines are then copied into chunks. That copy costs ~96 ms, more than half of the sequential counting time, and it is the main limit on the whole-program speedup. Chunks that point into one shared buffer (offsets or `std::string_view`) would remove it.
- `function_ratio`'s workload takes microseconds, so threads cannot make it faster; it demonstrates the pattern, not a speedup. Its `factorial` is a plain product for non-integers, not the gamma function.

## License

MIT, see [LICENSE](LICENSE).