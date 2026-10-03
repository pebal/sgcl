//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async::parallel_for against a plain loop and SNS-HDR's parallel_for_
// (Core/Parallel.h, the model of the async loops) on one kernel: a
// separable stack blur of a float image, as SNS-HDR's BlurFilter runs it —
// the rows spread over the lanes, then the columns in groups of eight (a
// loop with a step), each lane with a ring of its own (its scratch, indexed
// by the lane, by Parallel::threadNum() in SNS-HDR). The image is the same
// for every variant and every pixel is computed by the same code, so the
// checksum printed is the same for all of them.
//
//   bench_parallel_for <variant> blur [reps] [width] [height] [radius]
//       reps blurs of a width x height image (20, 4096, 4096, 8): the
//       median and the best ms per blur, the CPU time of the process
//   bench_parallel_for <variant> call [n] [indices]
//       n loops (20000) over `indices` indices (4 per lane) whose body
//       writes one word: the cost of a call, ns per loop (sgcltask: the
//       n loops made by one task)
//   bench_parallel_for <variant> check
//       every variant's blur of a small image against the plain loop's
//
// Variants: plain (one thread), sgcl (async::parallel_for, default grain),
// sgcl1 (grain 1: an index a claim, as SNS-HDR claims), sgcltask (the
// sgcl loops called inside a task), snshdr (parallel_for_ on
// Parallel::maxThreads() threads; built when -DSGCL_SNS_HDR_CORE=<SNS-HDR's
// Core directory> is given to cmake: Parallel.cpp needs nothing but the
// standard library). The lanes of sgcl are the scheduler's workers, one
// per core by default (SGCL_WORKERS), and SNS-HDR's threads one per core.
#include "benchmarks/common.h"
#include "sgcl/async.h"

#if defined(SGCL_BENCH_SNS_HDR)
#include "Parallel.h"
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    namespace async = sgcl::async;

    constexpr int Group = 8;   // the columns a vertical step blurs together

    // The stack blur of one line: out[x] = sum over k in -r..r of
    // (r + 1 - |k|) * in[x + k], over (r + 1)^2, the line's ends repeated
    // past them. Running sums: `sum` the weighted window, `sum_out` its
    // left half with the centre, `sum_in` its right half; `ring` the
    // window's 2r + 1 values, in[j] at j mod (2r + 1)
    struct Line {
        const float* in;
        float* out;
        ptrdiff_t stride;
        int n;

        float at(int j) const {
            return in[ptrdiff_t(std::clamp(j, 0, n - 1)) * stride];
        }
    };

    void blur_line(const Line& l, int r, float* ring) {
        const int div = 2 * r + 1;
        const float scale = 1.0f / float((r + 1) * (r + 1));
        float sum = 0, sum_in = 0, sum_out = 0;
        for (int k = -r; k <= r; ++k) {
            float v = l.at(k);
            ring[(k + div) % div] = v;
            sum += v * float(r + 1 - std::abs(k));
            (k <= 0 ? sum_out : sum_in) += v;
        }
        int old = r + 1;   // the slot of in[x - r]
        int mid = 1;       // the slot of in[x + 1]
        for (int x = 0; x < l.n; ++x) {
            l.out[ptrdiff_t(x) * l.stride] = sum * scale;
            sum -= sum_out;
            sum_out -= ring[old];
            float v = l.at(x + r + 1);
            ring[old] = v;
            sum_in += v;
            sum += sum_in;
            sum_out += ring[mid];
            sum_in -= ring[mid];
            if (++old == div) {
                old = 0;
            }
            if (++mid == div) {
                mid = 0;
            }
        }
    }

    // The same over Group neighbouring columns at once, rows apart: the
    // sums and the ring Group wide, so that a step reads and writes whole
    // cache lines of the row-major image
    void blur_columns(const float* in, float* out, int w, int h, int x0, int r, float* ring) {
        const int div = 2 * r + 1;
        const float scale = 1.0f / float((r + 1) * (r + 1));
        auto at = [&](int y) { return in + ptrdiff_t(std::clamp(y, 0, h - 1)) * w + x0; };
        float sum[Group] = {}, sum_in[Group] = {}, sum_out[Group] = {};
        for (int k = -r; k <= r; ++k) {
            const float* p = at(k);
            float* slot = ring + ((k + div) % div) * Group;
            const float weight = float(r + 1 - std::abs(k));
            for (int g = 0; g < Group; ++g) {
                slot[g] = p[g];
                sum[g] += p[g] * weight;
                (k <= 0 ? sum_out[g] : sum_in[g]) += p[g];
            }
        }
        int old = r + 1;
        int mid = 1;
        for (int y = 0; y < h; ++y) {
            float* o = out + ptrdiff_t(y) * w + x0;
            const float* p = at(y + r + 1);
            float* slot_old = ring + old * Group;
            const float* slot_mid = ring + mid * Group;
            for (int g = 0; g < Group; ++g) {
                o[g] = sum[g] * scale;
                sum[g] -= sum_out[g];
                sum_out[g] -= slot_old[g];
                slot_old[g] = p[g];
                sum_in[g] += p[g];
                sum[g] += sum_in[g];
            }
            for (int g = 0; g < Group; ++g) {
                sum_out[g] += slot_mid[g];
                sum_in[g] -= slot_mid[g];
            }
            if (++old == div) {
                old = 0;
            }
            if (++mid == div) {
                mid = 0;
            }
        }
    }

    struct Image {
        int w = 0, h = 0, r = 0;
        std::vector<float> src, tmp, dst;
        std::vector<float> rings;   // a ring of Group * (2r + 1) per lane
        int ring_size = 0;

        Image(int width, int height, int radius, unsigned lanes)
        : w(width), h(height), r(radius)
        , src(size_t(w) * h), tmp(size_t(w) * h), dst(size_t(w) * h)
        , ring_size(Group * (2 * radius + 1)) {
            uint32_t x = 2463534242u;
            for (auto& v : src) {   // xorshift: the same image every run
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                v = float(x >> 8) / float(1 << 24);
            }
            rings.resize(size_t(ring_size) * std::max(1u, lanes));
        }

        float* ring(unsigned lane) {
            return rings.data() + size_t(ring_size) * lane;
        }

        void row(int y, unsigned lane) {
            blur_line({src.data() + ptrdiff_t(y) * w, tmp.data() + ptrdiff_t(y) * w, 1, w}, r, ring(lane));
        }

        void columns(int x0, unsigned lane) {
            blur_columns(tmp.data(), dst.data(), w, h, x0, r, ring(lane));
        }

        double checksum() const {
            double s = 0;
            for (float v : dst) {
                s += v;
            }
            return s;
        }
    };

    unsigned lanes_of(const std::string& v) {
        if (v == "plain") {
            return 1;
        }
#if defined(SGCL_BENCH_SNS_HDR)
        if (v == "snshdr") {
            return Parallel::maxThreads();
        }
#endif
        return async::scheduler::workers();
    }

    // The same two loops called inside a task, on its worker
    async::task<> blur_in_task(Image& im, async::parallel_options o) {
        async::parallel_for(0, im.h, [&](int y, unsigned lane) { im.row(y, lane); }, o);
        async::parallel_for(0, im.w, Group, [&](int x0, unsigned lane) { im.columns(x0, lane); }, o);
        co_return;
    }

    void blur(const std::string& v, Image& im) {
        if (v == "plain") {
            for (int y = 0; y < im.h; ++y) {
                im.row(y, 0);
            }
            for (int x0 = 0; x0 < im.w; x0 += Group) {
                im.columns(x0, 0);
            }
        } else if (v == "sgcl" || v == "sgcl1") {
            async::parallel_options o{.grain = v == "sgcl1" ? size_t(1) : size_t(0)};
            async::parallel_for(0, im.h, [&](int y, unsigned lane) { im.row(y, lane); }, o);
            async::parallel_for(0, im.w, Group, [&](int x0, unsigned lane) { im.columns(x0, lane); }, o);
        } else if (v == "sgcltask") {
            async::spawn(blur_in_task(im, {})).wait();
#if defined(SGCL_BENCH_SNS_HDR)
        } else if (v == "snshdr") {
            parallel_for_(0, im.h, [&](int y) { im.row(y, unsigned(Parallel::threadNum())); });
            parallel_for_(0, im.w, Group, [&](int x0) { im.columns(x0, unsigned(Parallel::threadNum())); });
#endif
        } else {
            std::fprintf(stderr, "parallel_for: no variant %s\n", v.c_str());
            std::exit(2);
        }
    }

    void run_blur(const std::string& v, int reps, int w, int h, int r) {
        Image im(w, h, r, lanes_of(v));
        blur(v, im);   // the first: the pages touched, the threads started
        std::vector<double> ms;
        const double cpu0 = bench::cpu_seconds();
        const auto t0 = bench::Clock::now();
        for (int i = 0; i < reps; ++i) {
            const auto t = bench::Clock::now();
            blur(v, im);
            ms.push_back(bench::seconds_since(t) * 1e3);
        }
        const double wall = bench::seconds_since(t0);
        const double cpu = bench::cpu_seconds() - cpu0;
        std::sort(ms.begin(), ms.end());
        std::printf("parallel_for blur %s lanes=%u w=%d h=%d r=%d ms=%.3f best=%.3f wall=%.2fs cpu=%.2fs checksum=%.6e\n",
                    v.c_str(), lanes_of(v), w, h, r, ms[ms.size() / 2], ms.front(), wall, cpu, im.checksum());
    }

    void run_call(const std::string& v, long n, int indices) {
        const unsigned lanes = lanes_of(v);
        if (indices <= 0) {
            indices = int(lanes) * 4;
        }
        std::vector<uint64_t> words(size_t(indices) * 8);   // a word per index, a line apart
        auto body = [&](int i) { words[size_t(i) * 8] += 1; };
        auto one = [&] {
            if (v == "plain") {
                for (int i = 0; i < indices; ++i) {
                    body(i);
                }
            } else if (v == "sgcl" || v == "sgcltask") {   // sgcltask: called from the task below
                async::parallel_for(indices, body);
            } else if (v == "sgcl1") {
                async::parallel_for(indices, body, {.grain = 1});
#if defined(SGCL_BENCH_SNS_HDR)
            } else if (v == "snshdr") {
                parallel_for_(0, indices, body);
#endif
            } else {
                std::fprintf(stderr, "parallel_for: no variant %s\n", v.c_str());
                std::exit(2);
            }
        };
        auto loops = [&](long count) {
            for (long i = 0; i < count; ++i) {
                one();
            }
        };
        auto run = [&](long count) {
            if (v == "sgcltask") {
                async::spawn([&loops, count]() -> async::task<> {
                    loops(count);
                    co_return;
                }).wait();
            } else {
                loops(count);
            }
        };
        run(100);
        const double cpu0 = bench::cpu_seconds();
        const auto t0 = bench::Clock::now();
        run(n);
        const double wall = bench::seconds_since(t0);
        std::printf("parallel_for call %s lanes=%u indices=%d ns/op=%.1f wall=%.2fs cpu=%.2fs\n",
                    v.c_str(), lanes, indices, wall * 1e9 / double(n), wall, bench::cpu_seconds() - cpu0);
    }

    int run_check() {
        std::vector<std::string> variants = {"sgcl", "sgcl1", "sgcltask"};
#if defined(SGCL_BENCH_SNS_HDR)
        variants.push_back("snshdr");
#endif
        Image plain(256, 200, 5, 1);
        blur("plain", plain);
        int bad = 0;
        for (auto& v : variants) {
            Image im(256, 200, 5, lanes_of(v));
            blur(v, im);
            const bool same = im.dst == plain.dst;
            bad += !same;
            std::printf("parallel_for check %s %s\n", v.c_str(), same ? "ok" : "DIFFERS");
        }
        return bad ? 1 : 0;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: bench_parallel_for <plain|sgcl|sgcl1|sgcltask|snshdr> <blur [reps] [w] [h] [r] | call [n] [indices] | check>\n");
        return 2;
    }
    const std::string v = argv[1];
    const std::string what = argv[2];
    if (what == "blur") {
        const int reps = argc > 3 ? std::atoi(argv[3]) : 20;
        int w = argc > 4 ? std::atoi(argv[4]) : 4096;
        const int h = argc > 5 ? std::atoi(argv[5]) : 4096;
        const int r = argc > 6 ? std::atoi(argv[6]) : 8;
        w = std::max(Group, w / Group * Group);   // whole groups of columns
        run_blur(v, std::max(1, reps), w, std::max(1, h), std::max(1, r));
    } else if (what == "call") {
        run_call(v, argc > 3 ? std::atol(argv[3]) : 20000, argc > 4 ? std::atoi(argv[4]) : 0);
    } else if (what == "check") {
        return run_check();
    } else {
        std::fprintf(stderr, "parallel_for: no case %s\n", what.c_str());
        return 2;
    }
    return 0;
}
