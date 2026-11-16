// word_count_parallel: the same word count as ../word_count_solo, with the
// counting spread over N worker threads.
//
// Design:
//   * The chunks are built first, on the main thread, before any worker starts.
//   * Worker i gets its own contiguous range [begin, end) of chunk indices.
//   * Workers only READ `chunks` and only WRITE their own slot partial[i].
//   * The main thread reads the slots only after join(), so nothing is shared
//     and written at the same time.
// The result is checked against the sequential count on every run.
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

typedef std::chrono::duration<double, std::milli> MSEC;
typedef std::chrono::steady_clock HRC;  // monotonic clock, safe for measuring intervals


size_t countWords(const std::string& str)
{
    size_t count = 0;
    bool inWord = false;
    for (const char& c : str)
    {
        const bool s = (c == ' ' || (c >= '\t' && c <= '\r'));
        count += (!inWord && !s);
        inWord = !s;
    }

    return count;
}

size_t countWordsInChunks(const std::vector<std::string>& chunks) {
    size_t totalWords = 0;

    for (size_t i = 0; i < chunks.size(); i++) {
        totalWords += countWords(chunks[i]);
    }

    return totalWords;
}

// Counts all chunks with `threadCount` workers.
size_t countParallel(const std::vector<std::vector<std::string>>& chunks, size_t threadCount) {
    const size_t n = chunks.size();
    const size_t N = std::min(threadCount, n);   // never more workers than chunks
    if (N == 0) return 0;                        // empty input: nothing to do

    std::vector<size_t> partial(N, 0);           // one result slot per worker
    std::vector<std::thread> workers;
    workers.reserve(N);

    // Share n chunks among N workers: everyone gets n / N chunks, and the
    // n % N leftover chunks go one each to the first workers.
    // Example: 23 chunks, 4 workers -> [0,6) [6,12) [12,18) [18,23)
    const size_t base = n / N;
    const size_t extra = n % N;
    for (size_t i = 0; i < N; ++i) {
        const size_t begin = i * base + std::min(i, extra);
        const size_t end = begin + base + (i < extra ? 1 : 0);

        // i, begin and end are captured BY VALUE: they change / go out of scope
        // as the loop continues, so a reference could point to a wrong value.
        workers.emplace_back([&chunks, &partial, i, begin, end]() {
            size_t local = 0;                    // accumulate locally ...
            for (size_t c = begin; c < end; ++c) {
                local += countWordsInChunks(chunks[c]);
            }
            partial[i] = local;                  // ... and write the slot once
        });
    }

    // Start ALL workers first, then wait for them. (join() inside the loop
    // above would run the workers one after another.)
    for (std::thread& w : workers) {
        w.join();
    }

    size_t total = 0;
    for (size_t p : partial) {
        total += p;
    }
    return total;
}

// Runs fn() `repeats` times and returns the BEST time in msec (a single run is
// noisy: caches, allocator state and OS scheduling all vary). The result of
// the last run is stored in `result`.
template <class Fn>
double bestOf(int repeats, Fn fn, size_t& result) {
    double best = 1e300;
    for (int r = 0; r < repeats; ++r) {
        auto start = HRC::now();
        result = fn();
        best = std::min(best, std::chrono::duration_cast<MSEC>(HRC::now() - start).count());
    }
    return best;
}

int main() {
    std::string pat;
    std::cout << "Enter file name: ";
    std::cin >> pat;

    std::ifstream fin(pat);
    if (!fin.is_open()) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

    std::vector<std::string> textSplit;
    for (std::string line; std::getline(fin, line); ) {
        textSplit.push_back(line);
    }

    std::cout << "Enter a chunk size: ";
    size_t chunkSize{};
    if (!(std::cin >> chunkSize) || chunkSize == 0) {
        std::cerr << "Invalid chunk size (must be a positive integer)." << std::endl;
        return 1;
    }

    std::cout << "Enter number of threads (0 = auto): ";
    size_t threadCount{};
    if (!(std::cin >> threadCount)) {
        std::cerr << "Invalid thread count." << std::endl;
        return 1;
    }
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();   // may return 0 if unknown
        if (threadCount == 0) threadCount = 1;
    }

    size_t totalLength{ textSplit.size() };
    size_t chunkParts = totalLength / chunkSize + (totalLength % chunkSize != 0 ? 1 : 0);
    std::vector<std::vector<std::string>> chunks;
    chunks.reserve(chunkParts);

    // Step 1: split the lines into chunks (main thread)
    auto t0 = HRC::now();
    for (size_t i = 0; i < totalLength; i += chunkSize) {
        std::vector<std::string>::const_iterator start = textSplit.begin() + i;
        std::vector<std::string>::const_iterator end = textSplit.begin() + std::min(i + chunkSize, totalLength);

        chunks.push_back(std::vector<std::string>(start, end));
    }
    auto t1 = HRC::now();

    // Step 2: sequential count (the reference answer and the baseline time)
    const int REPEATS = 3;
    size_t expected{ 0 };
    const double seqMs = bestOf(REPEATS, [&]() {
        size_t sum = 0;
        for (size_t i = 0; i < chunks.size(); i++) {
            sum += countWordsInChunks(chunks[i]);
        }
        return sum;
    }, expected);

    // Step 3: parallel count
    size_t res{ 0 };
    const double parMs = bestOf(REPEATS, [&]() { return countParallel(chunks, threadCount); }, res);

    std::cout << "\nWords (parallel):   " << res << std::endl;
    std::cout << "Words (sequential): " << expected << std::endl;
    std::cout << "Results match:      " << (res == expected ? "yes" : "NO") << std::endl;
    std::cout << "Lines / chunks:     " << totalLength << " / " << chunks.size() << std::endl;
    std::cout << "Threads used:       " << std::min(threadCount, chunks.size()) << std::endl;
    std::cout << "Chunking time:      " << std::chrono::duration_cast<MSEC>(t1 - t0).count() << " msec" << std::endl;
    std::cout << "Sequential count:   " << seqMs << " msec (best of " << REPEATS << ")" << std::endl;
    std::cout << "Parallel count:     " << parMs << " msec (best of " << REPEATS << ")" << std::endl;
    if (parMs > 0) {
        std::cout << "Speedup:            " << seqMs / parMs << "x" << std::endl;
    }
    return res == expected ? 0 : 1;   // non-zero exit code if the check fails
}
