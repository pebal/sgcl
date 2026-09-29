//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A driver for harnesses written to libFuzzer's ABI
// (LLVMFuzzerTestOneInput), for machines without libFuzzer: it runs every
// seed file given, then mutates them — a bit flipped, bytes inserted,
// removed, overwritten with interesting values, a piece of another seed
// spliced in — for the seconds asked, with no coverage guidance. Built
// with -fsanitize=address,undefined, a crash is a finding; the input that
// caused it is written to crash-<n>.bin first, and an input that runs
// longer than ten seconds is one too (SIGALRM). One driver for every
// module's harnesses (tests/<module>/fuzz/<target>_fuzz.cpp), each of which
// says what it links:
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
//       -I<repo> tests/fuzz/driver.cpp tests/compress/fuzz/<target>_fuzz.cpp -lz -lbz2 -o fuzz
//   ASAN_OPTIONS=abort_on_error=1 ./fuzz <seconds> <seed files...>
//
// (abort_on_error makes ASan's report end in SIGABRT, which the handler
// here sees; -fno-sanitize-recover makes UBSan stop at its first report.)
// With libFuzzer present the harnesses link with -fsanitize=fuzzer instead
// of this file.
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

namespace {
    // the input being run, where a signal handler can reach it
    const uint8_t* volatile current_data = nullptr;
    volatile size_t current_size = 0;

    // Only calls a signal handler may make: open, write, close
    void save_current() {
        int fd = ::open("crash-0.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            const uint8_t* p = current_data;
            size_t left = current_size;
            while (left) {
                ssize_t w = ::write(fd, p, left);
                if (w <= 0) {
                    break;
                }
                p += w;
                left -= size_t(w);
            }
            ::close(fd);
        }
    }

    void run(const std::vector<uint8_t>& in) {
        current_data = in.data();
        current_size = in.size();
        ::alarm(10);
        LLVMFuzzerTestOneInput(in.data(), in.size());
        ::alarm(0);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "driver <seconds> <seed files...>\n");
        return 2;
    }
    std::set_terminate([] {
        save_current();
        std::abort();
    });
    // a trap (a property that failed), an abort (UBSan), a bad access: the input first
    for (int sig : {SIGTRAP, SIGABRT, SIGILL, SIGSEGV, SIGBUS, SIGALRM}) {
        std::signal(sig, [](int s) {
            save_current();
            std::signal(s, SIG_DFL);
            std::raise(s);
        });
    }
    double seconds = std::atof(argv[1]);
    std::vector<std::vector<uint8_t>> seeds;
    for (int i = 2; i < argc; ++i) {
        std::ifstream f(argv[i], std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        auto s = ss.str();
        seeds.emplace_back(s.begin(), s.end());
    }
    for (auto& s : seeds) {
        run(s);
    }
    std::mt19937_64 rng(std::random_device{}());
    static const uint8_t interesting[] = {0, 1, 0x7F, 0x80, 0xFF, 0xFE, 0x10, 0x20};
    auto start = std::chrono::steady_clock::now();
    uint64_t runs = 0;
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < seconds) {
        auto in = seeds[rng() % seeds.size()];
        int edits = 1 + int(rng() % 8);
        for (int k = 0; k < edits; ++k) {
            switch (rng() % 6) {
                case 0:
                    if (!in.empty()) in[rng() % in.size()] ^= uint8_t(1u << (rng() % 8));
                    break;
                case 1:
                    in.insert(in.begin() + std::ptrdiff_t(in.empty() ? 0 : rng() % in.size()), uint8_t(rng()));
                    break;
                case 2:
                    if (!in.empty()) in.erase(in.begin() + std::ptrdiff_t(rng() % in.size()));
                    break;
                case 3:
                    if (!in.empty()) in[rng() % in.size()] = interesting[rng() % sizeof(interesting)];
                    break;
                case 4: {
                    auto& other = seeds[rng() % seeds.size()];
                    if (!other.empty() && !in.empty()) {
                        size_t from = rng() % other.size(), len = 1 + rng() % std::min<size_t>(64, other.size() - from);
                        size_t at = rng() % in.size();
                        in.insert(in.begin() + std::ptrdiff_t(at), other.begin() + std::ptrdiff_t(from), other.begin() + std::ptrdiff_t(from + len));
                    }
                    break;
                }
                case 5:
                    if (in.size() > 1) in.resize(rng() % in.size());
                    break;
            }
        }
        run(in);
        ++runs;
    }
    std::printf("%llu runs in %.0f s, no crash\n", (unsigned long long)runs, seconds);
    return 0;
}
