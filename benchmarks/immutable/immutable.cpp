//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The immutable containers, one thread, against immer (the persistent
// vector and map of the same design over reference counts):
//
//   im vector sgcl [n=1000000]        push_back a version each, a random
//                                     read, set a version each, n elements
//                                     built at once (the range constructor)
//   im vector immer [n]               immer::vector, its default memory
//                                     policy: atomic reference counts,
//                                     thread-safe as sgcl's versions are;
//                                     built at once through a transient
//   im vector immer-unsafe [n]        immer::vector over non-atomic counts
//   im list sgcl|std [n=1000000]      push_front a version each, a walk of
//                                     the whole list, pop_front a version
//                                     each; std: std::forward_list in place
//                                     (immer has no list)
//   im map sgcl|immer|immer-unsafe|std [n=200000]
//                                     insert a version each over n random
//                                     long keys, find, n built at once;
//                                     std: std::map changing in place, the
//                                     price of a version against none
//   im map builder|immer-builder [n=200000]
//                                     the map's builder (thaw / freeze;
//                                     immer: transient / persistent): n
//                                     keys built through it, found, and a
//                                     tenth of them changed and a tenth
//                                     erased through it (edit) and one
//                                     version a change (edit_each)
//
// The immer variants need immer's headers: cmake -DSGCL_IMMER_INCLUDE=<dir>
// (the checkout's root); without it the binary has the sgcl variant only.
#include "sgcl/sgcl.h"
#include "benchmarks/common.h"

#include <cstdio>
#include <cstdlib>
#include <forward_list>
#include <map>
#include <random>
#include <string>
#include <vector>

#if __has_include(<immer/vector.hpp>)
#include <immer/map.hpp>
#include <immer/map_transient.hpp>
#include <immer/memory_policy.hpp>
#include <immer/vector.hpp>
#include <immer/vector_transient.hpp>
#define SGCL_HAS_IMMER 1
using unsafe_policy = immer::memory_policy<immer::default_heap_policy, immer::unsafe_refcount_policy, immer::no_lock_policy>;
#else
#define SGCL_HAS_IMMER 0
#endif

namespace {
    long sink = 0;

    // The vector: push_back, get, set, build; the best of three runs printed
    template<class Vec, class Push, class Get, class Set, class Build>
    void vector_case(const char* variant, long n, Push push, Get get, Set set, Build build) {
        double best[4] = {1e9, 1e9, 1e9, 1e9};
        for (int rep = 0; rep < 3; ++rep) {
            auto t0 = bench::Clock::now();
            Vec v;
            for (long i = 0; i < n; ++i) v = push(v, i);
            double t[4];
            t[0] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) sink += get(v, (i * 7919) % n);
            t[1] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) v = set(v, (i * 7919) % n, i);
            t[2] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            Vec b = build(n);
            t[3] = bench::seconds_since(t0);
            sink += get(b, n - 1);
            for (int k = 0; k < 4; ++k) best[k] = std::min(best[k], t[k]);
        }
        std::printf("vector %s n=%ld push_back=%.1f get=%.1f set=%.1f build=%.1f cpu=%.2fs\n", variant, n, best[0] * 1e9 / n, best[1] * 1e9 / n, best[2] * 1e9 / n, best[3] * 1e9 / n, bench::cpu_seconds());
    }

    template<class Map, class Insert, class Find, class Build>
    void map_case(const char* variant, const std::vector<long>& keys, Insert insert, Find find, Build build) {
        long n = keys.size();
        double best[3] = {1e9, 1e9, 1e9};
        for (int rep = 0; rep < 3; ++rep) {
            auto t0 = bench::Clock::now();
            Map m;
            for (long i = 0; i < n; ++i) m = insert(m, keys[i], i);
            double t[3];
            t[0] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) sink += find(m, keys[(i * 7919) % n]);
            t[1] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            Map b = build(keys);
            t[2] = bench::seconds_since(t0);
            sink += find(b, keys[0]);
            for (int k = 0; k < 3; ++k) best[k] = std::min(best[k], t[k]);
        }
        std::printf("map %s n=%ld insert=%.1f find=%.1f build=%.1f cpu=%.2fs\n", variant, n, best[0] * 1e9 / n, best[1] * 1e9 / n, best[2] * 1e9 / n, bench::cpu_seconds());
    }

    // The list: push_front, a walk, pop_front; the best of three runs
    template<class L, class Push, class Walk, class Pop>
    void list_case(const char* variant, long n, Push push, Walk walk, Pop pop) {
        double best[3] = {1e9, 1e9, 1e9};
        for (int rep = 0; rep < 3; ++rep) {
            auto t0 = bench::Clock::now();
            L l;
            for (long i = 0; i < n; ++i) l = push(l, i);
            double t[3];
            t[0] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            sink += walk(l);
            t[1] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) l = pop(l);
            t[2] = bench::seconds_since(t0);
            for (int k = 0; k < 3; ++k) best[k] = std::min(best[k], t[k]);
        }
        std::printf("list %s n=%ld push_front=%.1f walk=%.2f pop_front=%.1f cpu=%.2fs\n", variant, n, best[0] * 1e9 / n, best[1] * 1e9 / n, best[2] * 1e9 / n, bench::cpu_seconds());
    }

    // The map's builder against one insert a version at a time: n keys
    // built through thaw / insert / freeze, a lookup of each in the map
    // that comes out, and an edit of that map, a tenth of its keys given
    // new values and a tenth erased, through a builder (and one freeze)
    // and through the map's own insert and erase, ns per element or per
    // change; the best of three
    template<class Build, class Find, class Edit, class EditEach>
    void builder_case(const char* variant, const std::vector<long>& keys, Build build, Find find, Edit edit, EditEach edit_each) {
        long n = keys.size();
        double best[4] = {1e9, 1e9, 1e9, 1e9};
        for (int rep = 0; rep < 3; ++rep) {
            auto t0 = bench::Clock::now();
            auto m = build(keys);
            double t[4];
            t[0] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            for (long i = 0; i < n; ++i) sink += find(m, keys[(i * 7919) % n]);
            t[1] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            auto e = edit(m, keys);
            t[2] = bench::seconds_since(t0);
            t0 = bench::Clock::now();
            auto f = edit_each(m, keys);
            t[3] = bench::seconds_since(t0);
            sink += find(e, keys[1]) + find(f, keys[1]);
            for (int k = 0; k < 4; ++k) best[k] = std::min(best[k], t[k]);
        }
        long changes = 2 * (n / 10);
        std::printf("map %s n=%ld build=%.1f find=%.1f edit=%.1f edit_each=%.1f cpu=%.2fs\n", variant, n, best[0] * 1e9 / n, best[1] * 1e9 / n, best[2] * 1e9 / changes, best[3] * 1e9 / changes, bench::cpu_seconds());
    }

    std::vector<long> random_keys(long n) {
        std::mt19937_64 rng(99);
        std::vector<long> keys(n);
        for (long i = 0; i < n; ++i) keys[i] = long(rng() >> 2);
        return keys;
    }
}

int main(int argc, char** argv) {
    std::string what = argc > 1 ? argv[1] : "";
    std::string variant = argc > 2 ? argv[2] : "sgcl";
    if (what == "vector") {
        long n = argc > 3 ? std::atol(argv[3]) : 1000000;
        if (variant == "sgcl") {
            vector_case<sgcl::immutable::vector<long>>("sgcl", n,
                [](auto& v, long i) { return v.push_back(i); },
                [](auto& v, long i) { return v[i]; },
                [](auto& v, long i, long x) { return v.set(i, x); },
                [](long n) { std::vector<long> src(n); for (long i = 0; i < n; ++i) src[i] = i; return sgcl::immutable::vector<long>(src.begin(), src.end()); });
            return 0;
        }
#if SGCL_HAS_IMMER
        if (variant == "immer") {
            using V = immer::vector<long>;
            vector_case<V>("immer", n,
                [](auto& v, long i) { return v.push_back(i); },
                [](auto& v, long i) { return v[i]; },
                [](auto& v, long i, long x) { return v.set(i, x); },
                [](long n) { auto t = V{}.transient(); for (long i = 0; i < n; ++i) t.push_back(i); return t.persistent(); });
            return 0;
        }
        if (variant == "immer-unsafe") {
            using V = immer::vector<long, unsafe_policy>;
            vector_case<V>("immer-unsafe", n,
                [](auto& v, long i) { return v.push_back(i); },
                [](auto& v, long i) { return v[i]; },
                [](auto& v, long i, long x) { return v.set(i, x); },
                [](long n) { auto t = V{}.transient(); for (long i = 0; i < n; ++i) t.push_back(i); return t.persistent(); });
            return 0;
        }
#endif
    } else if (what == "list") {
        long n = argc > 3 ? std::atol(argv[3]) : 1000000;
        if (variant == "sgcl") {
            list_case<sgcl::immutable::list<long>>("sgcl", n,
                [](auto& l, long i) { return l.push_front(i); },
                [](auto& l) { long s = 0; for (long x : l) s += x; return s; },
                [](auto& l) { return l.pop_front(); });
            return 0;
        }
        if (variant == "std") {
            list_case<std::forward_list<long>>("std", n,
                [](auto& l, long i) { l.push_front(i); return std::move(l); },
                [](auto& l) { long s = 0; for (long x : l) s += x; return s; },
                [](auto& l) { l.pop_front(); return std::move(l); });
            return 0;
        }
    } else if (what == "map") {
        long n = argc > 3 ? std::atol(argv[3]) : 200000;
        auto keys = random_keys(n);
        if (variant == "std") {
            map_case<std::map<long, long>>("std", keys,
                [](auto& m, long k, long v) { m.emplace(k, v); return std::move(m); },
                [](auto& m, long k) { return m.find(k)->second; },
                [](const std::vector<long>& keys) { std::map<long, long> m; for (size_t i = 0; i < keys.size(); ++i) m.emplace(keys[i], (long)i); return m; });
            return 0;
        }
        if (variant == "builder") {
            using M = sgcl::immutable::map<long, long>;
            builder_case("builder", keys,
                [](const std::vector<long>& keys) { auto b = M().thaw(); for (size_t i = 0; i < keys.size(); ++i) b.insert(keys[i], (long)i); return b.freeze(); },
                [](auto& m, long k) { return *m.try_get(k); },
                [](const M& m, const std::vector<long>& keys) { auto b = m.thaw(); for (size_t i = 0; i < keys.size() / 10; ++i) { b.set(keys[i * 10], -1L); b.erase(keys[i * 10 + 5]); } return b.freeze(); },
                [](const M& m, const std::vector<long>& keys) { M e = m; for (size_t i = 0; i < keys.size() / 10; ++i) { e = e.set(keys[i * 10], -1L).erase(keys[i * 10 + 5]); } return e.set(keys[1], 0L); });
            return 0;
        }
        if (variant == "sgcl") {
            map_case<sgcl::immutable::map<long, long>>("sgcl", keys,
                [](auto& m, long k, long v) { return m.set(k, v); },
                [](auto& m, long k) { return *m.try_get(k); },
                [](const std::vector<long>& keys) { std::vector<std::pair<long, long>> r; r.reserve(keys.size()); for (size_t i = 0; i < keys.size(); ++i) r.emplace_back(keys[i], (long)i); return sgcl::immutable::map<long, long>(r.begin(), r.end()); });
            return 0;
        }
#if SGCL_HAS_IMMER
        if (variant == "immer-builder") {
            using M = immer::map<long, long>;
            builder_case("immer-builder", keys,
                [](const std::vector<long>& keys) { auto t = M{}.transient(); for (size_t i = 0; i < keys.size(); ++i) t.set(keys[i], (long)i); return t.persistent(); },
                [](auto& m, long k) { return *m.find(k); },
                [](const M& m, const std::vector<long>& keys) { auto t = m.transient(); for (size_t i = 0; i < keys.size() / 10; ++i) { t.set(keys[i * 10], -1L); t.erase(keys[i * 10 + 5]); } return t.persistent(); },
                [](const M& m, const std::vector<long>& keys) { M e = m; for (size_t i = 0; i < keys.size() / 10; ++i) { e = e.set(keys[i * 10], -1L).erase(keys[i * 10 + 5]); } return e.set(keys[1], 0L); });
            return 0;
        }
        if (variant == "immer") {
            using M = immer::map<long, long>;
            map_case<M>("immer", keys,
                [](auto& m, long k, long v) { return m.set(k, v); },
                [](auto& m, long k) { return *m.find(k); },
                [](const std::vector<long>& keys) { auto t = M{}.transient(); for (size_t i = 0; i < keys.size(); ++i) t.set(keys[i], (long)i); return t.persistent(); });
            return 0;
        }
        if (variant == "immer-unsafe") {
            using M = immer::map<long, long, std::hash<long>, std::equal_to<long>, unsafe_policy>;
            map_case<M>("immer-unsafe", keys,
                [](auto& m, long k, long v) { return m.set(k, v); },
                [](auto& m, long k) { return *m.find(k); },
                [](const std::vector<long>& keys) { auto t = M{}.transient(); for (size_t i = 0; i < keys.size(); ++i) t.set(keys[i], (long)i); return t.persistent(); });
            return 0;
        }
#endif
    }
    std::fprintf(stderr, "usage: bench_immutable vector sgcl|immer|immer-unsafe [n], bench_immutable list sgcl|std [n], bench_immutable map sgcl|immer|immer-unsafe|std|builder|immer-builder [n]%s\n", SGCL_HAS_IMMER ? "" : "   (built without immer: cmake -DSGCL_IMMER_INCLUDE=<dir>)");
    return 2;
}
