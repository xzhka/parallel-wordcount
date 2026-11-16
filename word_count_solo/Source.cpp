// word_count_solo: sequential baseline.
// Reads a text file, splits its lines into chunks of `chunkSize` lines and
// counts the words. No threads: this is the reference that the parallel
// version (../word_count_parallel) is compared against.
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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

int main()
{
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
    if (!(std::cin >> chunkSize) || chunkSize == 0) {   // validate BEFORE using it as a divisor / loop step
        std::cerr << "Invalid chunk size (must be a positive integer)." << std::endl;
        return 1;
    }

    size_t totalLength{ textSplit.size() };
    // number of chunks = ceil(totalLength / chunkSize), written without overflow
    size_t chunkParts = totalLength / chunkSize + (totalLength % chunkSize != 0 ? 1 : 0);
    std::vector<std::vector<std::string>> chunks;
    chunks.reserve(chunkParts);

    // Split the lines into chunks
    auto t0 = HRC::now();
    for (size_t i = 0; i < totalLength; i += chunkSize) {
        std::vector<std::string>::const_iterator start = textSplit.begin() + i;
        std::vector<std::string>::const_iterator end = textSplit.begin() + std::min(i + chunkSize, totalLength);

        chunks.push_back(std::vector<std::string>(start, end));
    }
    auto t1 = HRC::now();

    // Count the words in every chunk
    size_t res{ 0 };
    for (size_t i = 0; i < chunks.size(); i++)
    {
        res += countWordsInChunks(chunks[i]);
    }
    auto t2 = HRC::now();

    std::cout << "\nWords:          " << res << std::endl;
    std::cout << "Lines / chunks: " << totalLength << " / " << chunks.size() << std::endl;
    std::cout << "Chunking time:  " << std::chrono::duration_cast<MSEC>(t1 - t0).count() << " msec" << std::endl;
    std::cout << "Counting time:  " << std::chrono::duration_cast<MSEC>(t2 - t1).count() << " msec" << std::endl;
    return 0;
}
