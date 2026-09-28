//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The managed heap an operation of core, concurrent and immutable uses (the
// heap audit of 2026-09-26): the bytes allocated per operation with the
// collector parked (pages taken from the heap while nothing is swept, so
// every allocation shows), the objects an operation leaves alive (a full
// cycle's count), and the slot the bucket array of a hash table occupies.
// One line per case, named fields: case=..., n=..., then the measures.
//   bench_core_heap [case]      (every case without an argument)
#include "sgcl/core/core.h"
#include "sgcl/concurrent/concurrent.h"
#include "sgcl/immutable/immutable.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace sgcl;

namespace {
    volatile size_t sink = 0;

    // Managed bytes allocated per call of f, over n calls: the pages the
    // allocators took from the heap while the collector stood parked
    // (nothing swept, no slot reused), after a warm-up of n calls that
    // fills the free slots earlier sweeps left
    template<class F>
    double bytes_per_op(size_t n, F&& f) {
        using sgcl::detail::MemoryCounters;
        collector::force_collect(true);
        collector::force_collect(true);
        collector::stepper s(true);
        s.advance_to(collector::stepper::phase::start);
        for (size_t i = 0; i < n; ++i) {
            f();
        }
        auto p0 = MemoryCounters::alloc_since_cycle();
        for (size_t i = 0; i < n; ++i) {
            f();
        }
        auto p1 = MemoryCounters::alloc_since_cycle();
        s.finish_cycle();
        return double(p1 - p0) * double(config::page_size) / double(n);
    }

    // The objects a full cycle finds alive
    size_t live_objects() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        return collector::get_statistics().live_objects;
    }

    // The slot bytes of the live buffers of element type T (a range of
    // pages counts whole)
    template<class T>
    size_t buffer_bytes() {
        collector::force_collect(true);
        size_t bytes = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (*s.type == typeid(T[]) && s.buffers) {
                bytes += s.live_bytes;
            }
        }
        return bytes;
    }

    bool wanted(const char* name, const char* only) {
        return !only || std::string(name) == only;
    }

    // A function over a closure of one double: the value in the
    // function's buffer, or a node of its own
    void function_double(const char* only) {
        if (!wanted("function_double", only)) {
            return;
        }
        const size_t n = 200000;
        double b = bytes_per_op(n, [] {
            double x = double(sink);
            function<double()> f([x] { return x + 1; });
            sink += size_t(f());
        });
        size_t before = live_objects();
        size_t objects;
        {
            vector<function<double()>> kept;
            kept.reserve(1000);
            for (auto i : range(1000)) {
                double x = i;
                kept.emplace_back([x] { return x; });
            }
            objects = live_objects() - before;
            sink += kept.size();
        }
        std::printf("case=function_double n=%zu bytes/op=%.1f objects_per_1000=%zu\n", n, b, objects);
    }

    // An any holding a trivially copyable struct that is not trivially
    // default constructible (a default member initializer)
    struct Point {
        double x = 0;
        double y = 0;
    };

    void any_point(const char* only) {
        if (!wanted("any_point", only)) {
            return;
        }
        const size_t n = 200000;
        double b = bytes_per_op(n, [] {
            any a = Point{double(sink), 2};
            sink += size_t(any_cast<Point&>(a).y);
        });
        std::printf("case=any_point n=%zu bytes/op=%.1f\n", n, b);
    }

    // The bucket array of a hash map of n elements: the count, the bytes
    // it needs and the slot it occupies
    void hash_buckets(const char* only) {
        if (!wanted("hash_buckets", only)) {
            return;
        }
        for (size_t n : {1000u, 5000u, 8000u, 9000u, 16000u, 17000u, 33000u, 66000u, 131000u}) {
            map<long, long> m;
            for (auto i : range(long(n))) {
                m.emplace(i, i);
            }
            size_t slot = buffer_bytes<tracked_ptr<sgcl::detail::HashNodeBase>>();
            std::printf("case=hash_buckets n=%zu buckets=%zu bytes=%zu slot=%zu\n", n, m.bucket_count(), m.bucket_count() * sizeof(void*), slot);
        }
    }

    // A vector of ints made at a power of two of bytes past a page, then
    // pushed to four times that: the slot of its buffer at each step
    void vector_pow2(const char* only) {
        if (!wanted("vector_pow2", only)) {
            return;
        }
        vector<int> v(16384);   // 64 KB of elements
        std::printf("case=vector_pow2 size=%zu capacity=%zu slot=%zu\n", v.size(), v.capacity(), buffer_bytes<int>());
        while (v.size() < 4 * 16384) {
            auto c = v.capacity();
            while (v.capacity() == c) {
                v.push_back(1);
            }
            std::printf("case=vector_pow2 size=%zu capacity=%zu slot=%zu\n", v.size(), v.capacity(), buffer_bytes<int>());
        }
    }

    // Vectors of ints past a page, made at a size that is no power of
    // two: the slot of the buffer against the bytes it needs
    void vector_range(const char* only) {
        if (!wanted("vector_range", only)) {
            return;
        }
        for (size_t n : {20000u, 40000u, 100000u}) {
            size_t before = buffer_bytes<int>();
            vector<int> v(n);
            std::printf("case=vector_range size=%zu bytes=%zu capacity=%zu slot=%zu\n", n, n * sizeof(int) + sizeof(sgcl::detail::ArrayBase), v.capacity(), buffer_bytes<int>() - before);
        }
    }

    std::vector<std::pair<long, long>> pairs(size_t n) {
        std::vector<std::pair<long, long>> items;
        for (auto i : range(long(n))) {
            items.emplace_back(i * 7919, i);
        }
        return items;
    }

    // The containers built at once from a range of 10 000 pairs: every
    // managed byte the construction allocated, the result's included
    void from_range(const char* only) {
        auto items = pairs(10000);
        if (wanted("immutable_map_range", only)) {
            double b = bytes_per_op(20, [&] {
                immutable::map<long, long> m(items.begin(), items.end());
                sink += m.size();
            });
            std::printf("case=immutable_map_range n=10000 bytes/op=%.0f\n", b);
        }
        if (wanted("concurrent_map_range", only)) {
            double b = bytes_per_op(20, [&] {
                concurrent::map<long, long> m(items.begin(), items.end());
                sink += m.size();
            });
            std::printf("case=concurrent_map_range n=10000 bytes/op=%.0f\n", b);
        }
        if (wanted("concurrent_sorted_map_range", only)) {
            double b = bytes_per_op(20, [&] {
                concurrent::sorted_map<long, long> m(items.begin(), items.end());
                sink += m.size();
            });
            std::printf("case=concurrent_sorted_map_range n=10000 bytes/op=%.0f\n", b);
        }
    }

    // An empty concurrent map made and dropped
    void concurrent_map_empty(const char* only) {
        if (!wanted("concurrent_map_empty", only)) {
            return;
        }
        const size_t n = 20000;
        double b = bytes_per_op(n, [] {
            concurrent::map<long, long> m;
            sink += m.size();
        });
        std::printf("case=concurrent_map_empty n=%zu bytes/op=%.1f\n", n, b);
    }

    // The pieces of a string split by a separator given as a view
    void split_view(const char* only) {
        if (!wanted("split_view", only)) {
            return;
        }
        const size_t n = 200000;
        string csv("alpha, beta, gamma, delta");
        std::string sep(", ");
        double b = bytes_per_op(n, [&] {
            for (std::string_view piece : csv.split(std::string_view(sep))) {
                sink += piece.size();
            }
        });
        std::printf("case=split_view n=%zu bytes/op=%.1f\n", n, b);
    }

    // expected::value() on an error: the throw and the catch
    void expected_throw(const char* only) {
        if (!wanted("expected_throw", only)) {
            return;
        }
        const size_t n = 100000;
        expected<int, number_error> e = unexpected(number_error(number_error::reason::not_a_number, 3));
        double b = bytes_per_op(n, [&] {
            try {
                sink += size_t(e.value());
            } catch (const bad_expected_access<number_error>& x) {
                sink += x.error().offset();
            }
        });
        std::printf("case=expected_throw n=%zu bytes/op=%.1f\n", n, b);
    }

    // An empty forward_list made and dropped
    void forward_list_empty(const char* only) {
        if (!wanted("forward_list_empty", only)) {
            return;
        }
        const size_t n = 200000;
        double b = bytes_per_op(n, [] {
            forward_list<int> l;
            sink += l.empty();
        });
        std::printf("case=forward_list_empty n=%zu bytes/op=%.1f\n", n, b);
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* only = argc > 1 ? argv[1] : nullptr;
    function_double(only);
    any_point(only);
    hash_buckets(only);
    vector_pow2(only);
    vector_range(only);
    from_range(only);
    concurrent_map_empty(only);
    split_view(only);
    expected_throw(only);
    forward_list_empty(only);
    return sink == 42 ? 1 : 0;
}
