//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Latency of single mutator operations on a shared graph, as a distribution:
// the median is similar everywhere, the differences are in the tail.
//   graph_latency <sgcl|shared> [threads=cores] [seconds=5] [roots=4096]
// Each thread keeps a ring of roots. An operation is one of:
//   insert: a new node linked to four random existing nodes, stored into a
//           random root slot (the old subgraph may become garbage: freed
//           at once by shared_ptr in this thread, later by a collector);
//   walk:   from a random root, follow random links 32 steps, copying the
//           pointer into a local at every step (barrier / reference count).
// Every operation is timed; the end of the run drops all roots at once and
// times that too (the destructor cascade of shared_ptr, nothing for a GC).
// Prints per-operation percentiles in nanoseconds, the drop-all time, the
// operation rate, wall and process CPU time.
#include "common.h"
#include "sgcl/sgcl.h"

#include <atomic>
#include <memory>
#include <random>

namespace {
    constexpr int Links = 4;
    constexpr int WalkSteps = 32;

    struct Sgcl {
        struct Node {
            sgcl::tracked_ptr<Node> link[Links];
            long value;
            long pad[3];
        };
        using P = sgcl::tracked_ptr<Node>;
        using Roots = sgcl::vector<P>;
        static P make(long v) {
            auto n = sgcl::make_tracked<Node>();
            n->value = v;
            return n;
        }
    };

    struct Shared {
        struct Node {
            std::shared_ptr<Node> link[Links];
            long value;
            long pad[3];
        };
        using P = std::shared_ptr<Node>;
        using Roots = std::vector<P>;
        static P make(long v) {
            auto n = std::make_shared<Node>();
            n->value = v;
            return n;
        }
    };

    struct Result {
        std::vector<uint32_t> insert_ns;
        std::vector<uint32_t> walk_ns;
        long ops = 0;
        long checksum = 0;
        double drop_ns = 0;
    };

    template<class V>
    Result worker(int id, double seconds, int root_count) {
        using P = typename V::P;
        Result r;
        r.insert_ns.reserve(1 << 22);
        r.walk_ns.reserve(1 << 22);
        std::mt19937_64 rng(1234 + id);
        typename V::Roots roots;
        roots.resize(root_count);
        for (int i = 0; i < root_count; ++i) {
            roots[i] = V::make(i);
        }
        auto t0 = bench::Clock::now();
        long op = 0;
        for (;;) {
            if ((op & 1023) == 0 && bench::seconds_since(t0) >= seconds) {
                break;
            }
            ++op;
            auto start = bench::Clock::now();
            if (op % 4 == 0) {   // insert
                auto n = V::make(op);
                for (int l = 0; l < Links; ++l) {
                    n->link[l] = roots[rng() % root_count];
                }
                roots[rng() % root_count] = n;
                r.insert_ns.push_back((uint32_t)std::chrono::duration_cast<std::chrono::nanoseconds>(bench::Clock::now() - start).count());
            } else {             // walk
                P cur = roots[rng() % root_count];
                long sum = 0;
                for (int s = 0; s < WalkSteps; ++s) {
                    sum += cur->value;
                    P next = cur->link[rng() % Links];
                    if (!next) {
                        break;
                    }
                    cur = next;
                }
                r.checksum += sum;
                r.walk_ns.push_back((uint32_t)std::chrono::duration_cast<std::chrono::nanoseconds>(bench::Clock::now() - start).count());
            }
        }
        r.ops = op;
        auto drop = bench::Clock::now();
        roots.clear();
        roots.shrink_to_fit();
        r.drop_ns = std::chrono::duration<double, std::nano>(bench::Clock::now() - drop).count();
        return r;
    }

    template<class V>
    void run(int threads, double seconds, int root_count) {
        std::vector<Result> results(threads);
        std::vector<std::thread> ws;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] { results[t] = worker<V>(t, seconds, root_count); });
        }
        for (auto& w : ws) {
            w.join();
        }
        double wall = bench::seconds_since(t0);
        std::vector<uint32_t> insert, walk;
        long ops = 0;
        double drop_max = 0;
        long checksum = 0;
        for (auto& r : results) {
            insert.insert(insert.end(), r.insert_ns.begin(), r.insert_ns.end());
            walk.insert(walk.end(), r.walk_ns.begin(), r.walk_ns.end());
            ops += r.ops;
            drop_max = std::max(drop_max, r.drop_ns);
            checksum += r.checksum;
        }
        auto pi = bench::percentiles(insert);
        auto pw = bench::percentiles(walk);
        std::printf("insert p50=%.0f p90=%.0f p99=%.0f p99.9=%.0f p99.99=%.0f max=%.0f ns\n", pi.p50, pi.p90, pi.p99, pi.p999, pi.p9999, pi.max);
        std::printf("walk   p50=%.0f p90=%.0f p99=%.0f p99.9=%.0f p99.99=%.0f max=%.0f ns\n", pw.p50, pw.p90, pw.p99, pw.p999, pw.p9999, pw.max);
        std::printf("drop-all max=%.3f ms  ops/s=%.0f  wall=%.2fs cpu=%.2fs  (checksum %ld)\n", drop_max * 1e-6, ops / wall, wall, bench::cpu_seconds(), checksum);
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl", "shared"})) {
        std::fprintf(stderr, "usage: graph_latency <sgcl|shared> [threads] [seconds] [roots]\n");
        return 2;
    }
    int threads = argc > 2 ? std::atoi(argv[2]) : (int)bench::hardware_threads();
    double seconds = argc > 3 ? std::atof(argv[3]) : 5;
    int roots = argc > 4 ? std::atoi(argv[4]) : 4096;
    std::printf("%s threads=%d seconds=%.0f roots=%d\n", variant, threads, seconds, roots);
    if (!std::strcmp(variant, "sgcl")) run<Sgcl>(threads, seconds, roots);
    else run<Shared>(threads, seconds, roots);
}
