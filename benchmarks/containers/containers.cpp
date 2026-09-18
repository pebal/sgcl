//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The containers against their std counterparts: one case per process, so
// that the peak resident size belongs to that case alone.
//   containers <sgcl|std> <case> [n=1000000]
// cases: vector_push, vector_iterate, vector_ptr (tracked_ptr / shared_ptr
// elements), deque_push, deque_iterate, list_push, list_iterate,
// list_erase, forward_list_push, map_insert, map_find, map_iterate,
// set_insert, unordered_insert, unordered_find, unordered_erase,
// unordered_ptr. Prints ns per operation, wall, CPU, the container's
// footprint (what it occupies once the garbage of building it is gone:
// pages of live managed objects after a full collection, or bytes in use
// by malloc) and the peak RSS of the process.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

namespace {
    struct Node {
        long v;
    };

    struct SgclNode {
        long v;
        sgcl::tracked_ptr<SgclNode> next;
    };

    double peak_rss_mb() {
#if !defined(_WIN32)
        rusage ru;
        getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
        return ru.ru_maxrss / 1048576.0;
#else
        return ru.ru_maxrss / 1024.0;
#endif
#else
        return 0;
#endif
    }

    // The footprint: the difference between what is in use with the
    // container built and what was in use before it (keys and the like are
    // built before the mark). The std side asks malloc; the sgcl side
    // collects first, so that the garbage of building the container (old
    // buffers, replaced nodes) does not count.
    double malloc_in_use() {
#if defined(__APPLE__)
        malloc_statistics_t st;
        malloc_zone_statistics(nullptr, &st);
        return (double)st.size_in_use;
#elif defined(__GLIBC__)
        return (double)mallinfo2().uordblks;
#else
        return 0;
#endif
    }

    double managed_in_use() {
        sgcl::collector::force_collect(true);
        return (double)sgcl::detail::MemoryCounters::live_bytes();
    }

    double footprint_mark = 0;
    double footprint_mb = 0;
    double footprint_seconds = 0;   // spent measuring, taken out of the wall time
    bool sgcl_variant = true;

    void footprint_begin() {
        auto t0 = bench::Clock::now();
        footprint_mark = sgcl_variant ? managed_in_use() : malloc_in_use();
        footprint_seconds += bench::seconds_since(t0);
    }

    void footprint_end() {
        auto t0 = bench::Clock::now();
        footprint_mb = ((sgcl_variant ? managed_in_use() : malloc_in_use()) - footprint_mark) / 1048576.0;
        footprint_seconds += bench::seconds_since(t0);
    }

    std::vector<long> keys(long n) {
        std::vector<long> k(n);
        std::mt19937_64 rng(42);
        for (auto& x : k) {
            x = (long)(rng() >> 2);
        }
        return k;
    }

    template<class V>
    long run_vector_push(long n) {
        footprint_begin();
        V v;
        for (long i = 0; i < n; ++i) {
            v.push_back(i);
        }
        footprint_end();
        return v.size();
    }

    template<class V>
    long run_vector_iterate(long n) {
        footprint_begin();
        V v;
        for (long i = 0; i < n; ++i) {
            v.push_back(i);
        }
        long sum = 0;
        for (int r = 0; r < 20; ++r) {
            for (auto x : v) {
                sum += x;
            }
        }
        footprint_end();
        return sum;
    }

    template<class V, class P, class Make>
    long run_vector_ptr(long n, Make make) {
        footprint_begin();
        V v;
        for (long i = 0; i < n; ++i) {
            v.push_back(make(i));
        }
        long sum = 0;
        for (int r = 0; r < 5; ++r) {
            for (auto& p : v) {
                sum += p->v;
            }
            for (long i = 0; i + 1 < n; i += 2) {
                v[i] = v[i + 1];   // a pointer copy inside the buffer
            }
        }
        footprint_end();
        return sum;
    }

    template<class D>
    long run_deque_push(long n) {
        footprint_begin();
        D d;
        for (long i = 0; i < n; ++i) {
            if (i & 1) {
                d.push_back(i);
            } else {
                d.push_front(i);
            }
        }
        footprint_end();
        return d.size();
    }

    template<class D>
    long run_deque_iterate(long n) {
        footprint_begin();
        D d;
        for (long i = 0; i < n; ++i) {
            d.push_back(i);
        }
        long sum = 0;
        for (int r = 0; r < 20; ++r) {
            for (auto x : d) {
                sum += x;
            }
        }
        footprint_end();
        return sum;
    }

    template<class L>
    long run_list_push(long n) {
        footprint_begin();
        L l;
        for (long i = 0; i < n; ++i) {
            l.push_back(i);
        }
        footprint_end();
        return l.size();
    }

    template<class L>
    long run_list_iterate(long n) {
        footprint_begin();
        L l;
        for (long i = 0; i < n; ++i) {
            l.push_back(i);
        }
        long sum = 0;
        for (int r = 0; r < 20; ++r) {
            for (auto x : l) {
                sum += x;
            }
        }
        footprint_end();
        return sum;
    }

    template<class L>
    long run_list_erase(long n) {
        footprint_begin();
        L l;
        for (long i = 0; i < n; ++i) {
            l.push_back(i);
        }
        for (auto it = l.begin(); it != l.end();) {
            it = l.erase(it);
            if (it != l.end()) {
                ++it;
            }
        }
        footprint_end();
        return l.size();
    }

    template<class L>
    long run_forward_list_push(long n) {
        footprint_begin();
        L l;
        for (long i = 0; i < n; ++i) {
            l.push_front(i);
        }
        long sum = 0;
        for (auto x : l) {
            sum += x;
        }
        footprint_end();
        return sum;
    }

    template<class M>
    long run_map_insert(long n) {
        auto k = keys(n);
        footprint_begin();
        M m;
        for (long i = 0; i < n; ++i) {
            m.emplace(k[i], i);
        }
        footprint_end();
        return m.size();
    }

    template<class M>
    long run_map_find(long n) {
        auto k = keys(n);
        footprint_begin();
        M m;
        for (long i = 0; i < n; ++i) {
            m.emplace(k[i], i);
        }
        long hits = 0;
        for (int r = 0; r < 3; ++r) {
            for (long i = 0; i < n; ++i) {
                hits += m.find(k[i]) != m.end();
            }
        }
        footprint_end();
        return hits;
    }

    template<class M>
    long run_map_iterate(long n) {
        auto k = keys(n);
        footprint_begin();
        M m;
        for (long i = 0; i < n; ++i) {
            m.emplace(k[i], i);
        }
        long sum = 0;
        for (int r = 0; r < 5; ++r) {
            for (auto& [key, value] : m) {
                sum += value;
            }
        }
        footprint_end();
        return sum;
    }

    template<class S>
    long run_set_insert(long n) {
        auto k = keys(n);
        footprint_begin();
        S s;
        for (long i = 0; i < n; ++i) {
            s.insert(k[i]);
        }
        footprint_end();
        return s.size();
    }

    template<class M>
    long run_unordered_erase(long n) {
        auto k = keys(n);
        footprint_begin();
        M m;
        for (long i = 0; i < n; ++i) {
            m.emplace(k[i], i);
        }
        for (long i = 0; i < n; i += 2) {
            m.erase(k[i]);
        }
        footprint_end();
        return m.size();
    }

    template<class M, class P, class Make>
    long run_unordered_ptr(long n, Make make) {
        auto k = keys(n);
        footprint_begin();
        M m;
        for (long i = 0; i < n; ++i) {
            m.emplace(k[i], make(i));
        }
        long sum = 0;
        for (long i = 0; i < n; ++i) {
            sum += m.find(k[i])->second->v;
        }
        footprint_end();
        return sum;
    }

    // Operations counted per case: the loop that the case is about.
    long ops_of(const std::string& c, long n) {
        if (c == "vector_iterate" || c == "deque_iterate" || c == "list_iterate") return n * 20;
        if (c == "vector_ptr") return n * 5;
        if (c == "map_find") return n * 3;
        if (c == "map_iterate") return n * 5;
        if (c == "list_erase") return n;
        if (c == "unordered_erase") return n / 2;
        if (c == "unordered_ptr") return n * 2;
        return n;
    }
}

int main(int argc, char** argv) {
    std::string variant = argc > 1 ? argv[1] : "sgcl";
    std::string c = argc > 2 ? argv[2] : "vector_push";
    long n = argc > 3 ? std::atol(argv[3]) : 1'000'000;
    bool sg = variant == "sgcl";
    sgcl_variant = sg;
    auto t0 = bench::Clock::now();
    long result = 0;
    auto make_tracked = [](long i) { auto p = sgcl::make_tracked<Node>(); p->v = i; return p; };
    auto make_shared = [](long i) { auto p = std::make_shared<Node>(); p->v = i; return p; };
#define PICK(S, D) (sg ? (S) : (D))
    if (c == "vector_push") result = PICK(run_vector_push<sgcl::vector<long>>(n), run_vector_push<std::vector<long>>(n));
    else if (c == "vector_iterate") result = PICK(run_vector_iterate<sgcl::vector<long>>(n), run_vector_iterate<std::vector<long>>(n));
    else if (c == "vector_ptr") result = PICK((run_vector_ptr<sgcl::vector<sgcl::tracked_ptr<Node>>, sgcl::tracked_ptr<Node>>(n, make_tracked)),
                                              (run_vector_ptr<std::vector<std::shared_ptr<Node>>, std::shared_ptr<Node>>(n, make_shared)));
    else if (c == "deque_push") result = PICK(run_deque_push<sgcl::deque<long>>(n), run_deque_push<std::deque<long>>(n));
    else if (c == "deque_iterate") result = PICK(run_deque_iterate<sgcl::deque<long>>(n), run_deque_iterate<std::deque<long>>(n));
    else if (c == "list_push") result = PICK(run_list_push<sgcl::list<long>>(n), run_list_push<std::list<long>>(n));
    else if (c == "list_iterate") result = PICK(run_list_iterate<sgcl::list<long>>(n), run_list_iterate<std::list<long>>(n));
    else if (c == "list_erase") result = PICK(run_list_erase<sgcl::list<long>>(n), run_list_erase<std::list<long>>(n));
    else if (c == "forward_list_push") result = PICK(run_forward_list_push<sgcl::forward_list<long>>(n), run_forward_list_push<std::forward_list<long>>(n));
    else if (c == "map_insert") result = PICK((run_map_insert<sgcl::map<long, long>>(n)), (run_map_insert<std::map<long, long>>(n)));
    else if (c == "map_find") result = PICK((run_map_find<sgcl::map<long, long>>(n)), (run_map_find<std::map<long, long>>(n)));
    else if (c == "map_iterate") result = PICK((run_map_iterate<sgcl::map<long, long>>(n)), (run_map_iterate<std::map<long, long>>(n)));
    else if (c == "set_insert") result = PICK(run_set_insert<sgcl::set<long>>(n), run_set_insert<std::set<long>>(n));
    else if (c == "unordered_insert") result = PICK((run_map_insert<sgcl::unordered_map<long, long>>(n)), (run_map_insert<std::unordered_map<long, long>>(n)));
    else if (c == "unordered_find") result = PICK((run_map_find<sgcl::unordered_map<long, long>>(n)), (run_map_find<std::unordered_map<long, long>>(n)));
    else if (c == "unordered_erase") result = PICK((run_unordered_erase<sgcl::unordered_map<long, long>>(n)), (run_unordered_erase<std::unordered_map<long, long>>(n)));
    else if (c == "unordered_ptr") result = PICK((run_unordered_ptr<sgcl::unordered_map<long, sgcl::tracked_ptr<Node>>, sgcl::tracked_ptr<Node>>(n, make_tracked)),
                                                 (run_unordered_ptr<std::unordered_map<long, std::shared_ptr<Node>>, std::shared_ptr<Node>>(n, make_shared)));
#undef PICK
    else {
        std::fprintf(stderr, "unknown case %s\n", c.c_str());
        return 2;
    }
    double wall = bench::seconds_since(t0) - footprint_seconds;
    std::printf("%s %s n=%ld ns/op=%.1f wall=%.2fs cpu=%.2fs footprint=%.1fMB rss=%.0fMB (result %ld)\n", variant.c_str(), c.c_str(), n, wall * 1e9 / ops_of(c, n), wall, bench::cpu_seconds(), footprint_mb, peak_rss_mb(), result);
}
