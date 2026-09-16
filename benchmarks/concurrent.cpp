//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The lock-free containers shared by every thread: concurrent_queue
// (Michael–Scott), concurrent_stack (Treiber), concurrent_map and
// concurrent_set (a skip list) and concurrent_unordered_map (a
// split-ordered list), against what C++ has without a collector and
// against Go and Java (benchmarks/go/concurrent,
// benchmarks/java/Concurrent.java: the same shapes, Java's
// ConcurrentLinkedQueue, ConcurrentLinkedDeque, ConcurrentSkipListMap,
// ConcurrentSkipListSet and ConcurrentHashMap). An element is an Item
// object of one long: what the containers hold in every language, a
// pointer to an object; the set holds the longs.
//   concurrent <queue|stack> <sgcl|mutex|shared> [threads=4] [mode=mixed] [n=200000]
//   concurrent <map|umap|set> <sgcl|mutex|rwlock> [threads=4] [keys=200000] [n=200000]
//   concurrent cow <sgcl|shared|rwlock> [threads=16] [n=2000000]
//   concurrent chan <sgcl|mutex> [threads=4] [capacity=64] [n=200000]
// queue, stack: mixed, every thread pushes an item and pops one, n times
// over; pairs, half the threads push n items each, the other half pop n
// each. mutex: the std container of shared_ptr under a std::mutex, the
// classic answer without a collector; shared: the same lock-free
// algorithm on the standard library's atomic operations on shared_ptr
// (a lock inside), as far as they take it.
// map, umap, set: three phases, each timed on its own: insert, the
// threads insert `keys` disjoint keys; find, every thread looks up n
// random keys of those; mixed, every thread does n operations over twice
// the key range, 80% lookups, 10% insertions, 10% erasures. mutex: the
// std container (std::map, std::unordered_map of shared_ptr, std::set)
// under a std::mutex; rwlock: the same under a std::shared_mutex, the
// lookups as readers.
// cow: copy_on_write over an array of 64 longs; threads - 1 readers each
// take a snapshot and sum it n times, one writer replaces the value
// (a copy with one element changed) as fast as it can meanwhile. shared:
// std::shared_ptr<const array> with the atomic operations of <memory>;
// rwlock: the array under a std::shared_mutex, the readers as readers,
// the writer changing it in place.
// chan: a channel of the given capacity (0: a rendezvous) between
// threads / 2 producers, each sending n items, and threads / 2
// consumers; mutex: std::queue under a std::mutex with two condition
// variables, the classic bounded queue. Go: its channel; Java:
// ArrayBlockingQueue, SynchronousQueue for capacity 0.
// Prints nanoseconds per operation and the process CPU time.
#include "common.h"
#include "sgcl/sgcl.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <shared_mutex>
#include <stack>
#include <unordered_map>
#include <thread>
#include <vector>

namespace {
    struct Item {
        long value;
    };

    struct SgclQueue {
        sgcl::concurrent_queue<sgcl::tracked_ptr<Item>> q;
        void push(long v) {
            q.emplace(sgcl::make_tracked<Item>(v));
        }
        long pop() {
            auto p = q.try_pop();
            return p ? (*p)->value : -1;
        }
    };

    struct SgclStack {
        sgcl::concurrent_stack<sgcl::tracked_ptr<Item>> s;
        void push(long v) {
            s.emplace(sgcl::make_tracked<Item>(v));
        }
        long pop() {
            auto p = s.try_pop();
            return p ? (*p)->value : -1;
        }
    };

    struct SgclMap {
        sgcl::concurrent_map<long, sgcl::tracked_ptr<Item>> m;
        bool insert(long k) {
            return m.try_emplace(k, sgcl::make_tracked<Item>(k)).second;
        }
        long find(long k) {
            auto it = m.find(k);
            return it != m.end() ? it->second->value : -1;
        }
        bool erase(long k) {
            return m.erase(k) != 0;
        }
    };

    struct MutexQueue {
        std::queue<std::shared_ptr<Item>> q;
        std::mutex mutex;
        void push(long v) {
            auto p = std::make_shared<Item>(v);
            std::lock_guard lock(mutex);
            q.push(std::move(p));
        }
        long pop() {
            std::shared_ptr<Item> p;
            {
                std::lock_guard lock(mutex);
                if (q.empty()) {
                    return -1;
                }
                p = std::move(q.front());
                q.pop();
            }
            return p->value;
        }
    };

    struct MutexStack {
        std::stack<std::shared_ptr<Item>> s;
        std::mutex mutex;
        void push(long v) {
            auto p = std::make_shared<Item>(v);
            std::lock_guard lock(mutex);
            s.push(std::move(p));
        }
        long pop() {
            std::shared_ptr<Item> p;
            {
                std::lock_guard lock(mutex);
                if (s.empty()) {
                    return -1;
                }
                p = std::move(s.top());
                s.pop();
            }
            return p->value;
        }
    };

    // The Michael–Scott queue on shared_ptr: the head, the tail and every
    // link an atomic shared_ptr through the free functions of <memory>,
    // which the standard library implements with a lock. The element is
    // read before the head is swung, as the paper has it, since the node
    // may be freed by another consumer's release right after.
    struct SharedQueue {
        struct Node {
            std::shared_ptr<Node> next;
            std::unique_ptr<Item> item;
        };
        std::shared_ptr<Node> head = std::make_shared<Node>();
        std::shared_ptr<Node> tail = head;
        void push(long v) {
            auto n = std::make_shared<Node>();
            n->item = std::make_unique<Item>(v);
            for (;;) {
                auto t = std::atomic_load(&tail);
                auto next = std::atomic_load(&t->next);
                if (next) {
                    std::atomic_compare_exchange_weak(&tail, &t, next);
                    continue;
                }
                std::shared_ptr<Node> null;
                if (std::atomic_compare_exchange_weak(&t->next, &null, n)) {
                    std::atomic_compare_exchange_strong(&tail, &t, n);
                    return;
                }
            }
        }
        long pop() {
            for (;;) {
                auto h = std::atomic_load(&head);
                auto next = std::atomic_load(&h->next);
                if (!next) {
                    return -1;
                }
                auto t = std::atomic_load(&tail);
                if (h == t) {
                    std::atomic_compare_exchange_weak(&tail, &t, next);
                    continue;
                }
                long v = next->item->value;
                if (std::atomic_compare_exchange_weak(&head, &h, next)) {
                    return v;
                }
            }
        }
    };

    // the backoff of the SGCL stack, so that the algorithms are the same
    struct SharedStack {
        struct Node {
            std::shared_ptr<Node> next;
            std::unique_ptr<Item> item;
        };
        std::shared_ptr<Node> head;
        void push(long v) {
            auto n = std::make_shared<Node>();
            n->item = std::make_unique<Item>(v);
            n->next = std::atomic_load(&head);
            sgcl::detail::Backoff backoff;
            while (!std::atomic_compare_exchange_weak(&head, &n->next, n)) {
                backoff();
            }
        }
        long pop() {
            auto h = std::atomic_load(&head);
            sgcl::detail::Backoff backoff;
            while (h && !std::atomic_compare_exchange_weak(&head, &h, h->next)) {
                backoff();
            }
            return h ? h->item->value : -1;
        }
    };

    struct SgclUmap {
        sgcl::concurrent_unordered_map<long, sgcl::tracked_ptr<Item>> m;
        bool insert(long k) {
            return m.try_emplace(k, sgcl::make_tracked<Item>(k)).second;
        }
        long find(long k) {
            auto it = m.find(k);
            return it != m.end() ? it->second->value : -1;
        }
        bool erase(long k) {
            return m.erase(k) != 0;
        }
    };

    struct SgclSet {
        sgcl::concurrent_set<long> s;
        bool insert(long k) {
            return s.insert(k).second;
        }
        long find(long k) {
            return s.contains(k) ? k : -1;
        }
        bool erase(long k) {
            return s.erase(k) != 0;
        }
    };

    // the std container under a lock: M the container, Mutex the lock,
    // Reader the guard of a lookup (a shared_lock under a shared_mutex)
    template<class M, class Mutex, template<class> class Reader>
    struct Locked {
        M m;
        Mutex mutex;
        bool insert(long k) {
            if constexpr (requires { m.try_emplace(k, std::shared_ptr<Item>()); }) {
                auto p = std::make_shared<Item>(k);
                std::unique_lock lock(mutex);
                return m.try_emplace(k, std::move(p)).second;
            } else {
                std::unique_lock lock(mutex);
                return m.insert(k).second;
            }
        }
        long find(long k) {
            Reader<Mutex> lock(mutex);
            auto it = m.find(k);
            if (it == m.end()) {
                return -1;
            }
            if constexpr (requires { it->second; }) {
                return it->second->value;
            } else {
                return *it;
            }
        }
        bool erase(long k) {
            std::unique_lock lock(mutex);
            auto it = m.find(k);
            if (it == m.end()) {
                return false;
            }
            if constexpr (requires { it->second; }) {
                auto p = std::move(it->second);   // the Item released outside the lock
                m.erase(it);
                lock.unlock();
                return true;
            } else {
                m.erase(it);
                return true;
            }
        }
    };

    using Values = std::array<long, 64>;

    struct SgclCow {
        sgcl::copy_on_write<Values> v;
        SgclCow() : v(Values{}) {}
        long read() {
            auto s = v.load();
            long sum = 0;
            for (long x : *s) {
                sum += x;
            }
            return sum;
        }
        void write(long i) {
            v.update([i](Values& a) { a[size_t(i % 64)] = (a[size_t(i % 64)] + 1) % 100; });
        }
    };

    struct SharedCow {
        std::shared_ptr<const Values> v = std::make_shared<const Values>();
        long read() {
            auto s = std::atomic_load(&v);
            long sum = 0;
            for (long x : *s) {
                sum += x;
            }
            return sum;
        }
        void write(long i) {
            auto old = std::atomic_load(&v);
            for (;;) {
                auto next = std::make_shared<Values>(*old);
                (*next)[size_t(i % 64)] = ((*next)[size_t(i % 64)] + 1) % 100;
                std::shared_ptr<const Values> desired = std::move(next);
                if (std::atomic_compare_exchange_strong(&v, &old, desired)) {
                    return;
                }
            }
        }
    };

    struct RwlockCow {
        Values v = {};
        std::shared_mutex mutex;
        long read() {
            std::shared_lock lock(mutex);
            long sum = 0;
            for (long x : v) {
                sum += x;
            }
            return sum;
        }
        void write(long i) {
            std::unique_lock lock(mutex);
            v[size_t(i % 64)] = (v[size_t(i % 64)] + 1) % 100;
        }
    };

    template<class C>
    void run_cow(int threads, long n) {
        C c;
        std::vector<std::thread> ws;
        sgcl::atomic<bool> stop = {false};
        sgcl::atomic<long> writes = {0};
        double write_time = 0;
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads - 1; ++t) {
            ws.emplace_back([&] {
                long sum = 0;
                for (long i = 0; i < n; ++i) {
                    sum += c.read();
                }
                if (sum == -1) {
                    std::printf("?");
                }
            });
        }
        std::thread writer([&] {
            auto w0 = bench::Clock::now();
            long i = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                c.write(i++);
            }
            writes = i;
            write_time = bench::seconds_since(w0);
        });
        for (auto& w : ws) {
            w.join();
        }
        stop = true;
        writer.join();
        double wall = bench::seconds_since(t0);
        double reads = (double)n * (threads - 1);
        std::printf("cow threads=%d ns/read=%.1f ns/write=%.1f writes=%ld reads/s=%.0f wall=%.2fs cpu=%.2fs\n", threads, wall * 1e9 / reads, writes ? write_time * 1e9 / (double)writes : 0.0, writes.load(), reads / wall, wall, bench::cpu_seconds());
    }

    struct SgclChan {
        sgcl::channel<sgcl::tracked_ptr<Item>> ch;
        explicit SgclChan(size_t cap) : ch(cap) {}
        void send(long v) {
            ch.send(sgcl::make_tracked<Item>(v));
        }
        long receive() {
            auto p = ch.receive();
            return p ? (*p)->value : -1;
        }
        void close() {
            ch.close();
        }
    };

    // the classic bounded queue: a mutex, a queue, a condition variable
    // for each side; capacity 0 taken as 1
    struct MutexChan {
        std::queue<std::shared_ptr<Item>> q;
        std::mutex mutex;
        std::condition_variable not_empty, not_full;
        size_t cap;
        bool closed = false;
        explicit MutexChan(size_t c) : cap(c ? c : 1) {}
        void send(long v) {
            auto p = std::make_shared<Item>(v);
            std::unique_lock lock(mutex);
            not_full.wait(lock, [&] { return q.size() < cap; });
            q.push(std::move(p));
            not_empty.notify_one();
        }
        long receive() {
            std::shared_ptr<Item> p;
            {
                std::unique_lock lock(mutex);
                not_empty.wait(lock, [&] { return !q.empty() || closed; });
                if (q.empty()) {
                    return -1;
                }
                p = std::move(q.front());
                q.pop();
                not_full.notify_one();
            }
            return p->value;
        }
        void close() {
            std::lock_guard lock(mutex);
            closed = true;
            not_empty.notify_all();
        }
    };

    template<class C>
    void run_chan(int threads, size_t cap, long n) {
        C c(cap);
        int producers = std::max(1, threads / 2), consumers = std::max(1, threads / 2);
        std::vector<std::thread> ws;
        sgcl::atomic<int> done = {0};
        auto t0 = bench::Clock::now();
        for (int t = 0; t < producers; ++t) {
            ws.emplace_back([&] {
                for (long i = 0; i < n; ++i) {
                    c.send(i);
                }
                if (++done == producers) {
                    c.close();
                }
            });
        }
        for (int t = 0; t < consumers; ++t) {
            ws.emplace_back([&] {
                long sum = 0;
                for (;;) {
                    long v = c.receive();
                    if (v < 0) {
                        break;
                    }
                    sum += v;
                }
                if (sum == -1) {
                    std::printf("?");
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        double wall = bench::seconds_since(t0);
        double ops = (double)n * producers;
        std::printf("chan threads=%d capacity=%zu ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", threads, cap, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    template<class M> using Exclusive = std::unique_lock<M>;
    template<class M> using Shared = std::shared_lock<M>;

    struct MutexMap {
        std::map<long, std::shared_ptr<Item>> m;
        std::mutex mutex;
        bool insert(long k) {
            auto p = std::make_shared<Item>(k);
            std::lock_guard lock(mutex);
            return m.try_emplace(k, std::move(p)).second;
        }
        long find(long k) {
            std::lock_guard lock(mutex);
            auto it = m.find(k);
            return it != m.end() ? it->second->value : -1;
        }
        bool erase(long k) {
            std::shared_ptr<Item> p;
            std::lock_guard lock(mutex);
            auto it = m.find(k);
            if (it == m.end()) {
                return false;
            }
            p = std::move(it->second);
            m.erase(it);
            return true;
        }
    };

    struct RwlockMap {
        std::map<long, std::shared_ptr<Item>> m;
        std::shared_mutex mutex;
        bool insert(long k) {
            auto p = std::make_shared<Item>(k);
            std::unique_lock lock(mutex);
            return m.try_emplace(k, std::move(p)).second;
        }
        long find(long k) {
            std::shared_lock lock(mutex);
            auto it = m.find(k);
            return it != m.end() ? it->second->value : -1;
        }
        bool erase(long k) {
            std::shared_ptr<Item> p;
            std::unique_lock lock(mutex);
            auto it = m.find(k);
            if (it == m.end()) {
                return false;
            }
            p = std::move(it->second);
            m.erase(it);
            return true;
        }
    };

    // queue and stack: the loops of benchmarks/lockfree_stack.cpp
    template<class C>
    void run_container(const char* what, int threads, const std::string& mode, long n) {
        C c;
        std::vector<std::thread> ws;
        bool pairs = mode == "pairs";
        auto t0 = bench::Clock::now();
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                long sum = 0;
                if (!pairs) {
                    for (long i = 0; i < n; ++i) {
                        c.push(i);
                        sum += c.pop();
                    }
                } else if (t % 2 == 0) {
                    for (long i = 0; i < n; ++i) {
                        c.push(i);
                    }
                } else {
                    for (long i = 0; i < n;) {
                        auto v = c.pop();
                        if (v >= 0) {
                            sum += v;
                            ++i;
                        } else {
                            std::this_thread::yield();
                        }
                    }
                }
                if (sum == -1) {
                    std::printf("?");
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        double wall = bench::seconds_since(t0);
        double ops = pairs ? (double)n * threads : 2.0 * n * threads;
        std::printf("%s threads=%d mode=%s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, threads, mode.c_str(), wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds());
    }

    template<class M>
    void run_map(const char* what, int threads, long keys, long n) {
        M m;
        std::vector<std::thread> ws;
        auto phase = [&](auto&& body) {
            auto t0 = bench::Clock::now();
            for (int t = 0; t < threads; ++t) {
                ws.emplace_back([&, t] { body(t); });
            }
            for (auto& w : ws) {
                w.join();
            }
            ws.clear();
            return bench::seconds_since(t0);
        };
        // insert: disjoint keys, interleaved between the threads
        double insert = phase([&](int t) {
            for (long k = t; k < keys; k += threads) {
                m.insert(k);
            }
        });
        // find: random keys of those, every one present
        double find = phase([&](int t) {
            std::mt19937_64 rng(1234 + t);
            long sum = 0;
            for (long i = 0; i < n; ++i) {
                sum += m.find(long(rng() % uint64_t(keys)));
            }
            if (sum == -1) {
                std::printf("?");
            }
        });
        // mixed: 80% lookups, 10% insertions, 10% erasures, over twice the range
        double mixed = phase([&](int t) {
            std::mt19937_64 rng(4321 + t);
            long sum = 0;
            for (long i = 0; i < n; ++i) {
                auto r = rng();
                long k = long((r >> 8) % uint64_t(2 * keys));
                auto op = r & 0xFF;
                if (op < 205) {
                    sum += m.find(k);
                } else if (op < 230) {
                    sum += m.insert(k);
                } else {
                    sum += m.erase(k);
                }
            }
            if (sum == -1) {
                std::printf("?");
            }
        });
        double ops = (double)n * threads;
        std::printf("%s threads=%d keys=%ld insert=%.1f find=%.1f mixed=%.1f wall=%.2fs cpu=%.2fs\n", what, threads, keys, insert * 1e9 / (double)keys, find * 1e9 / ops, mixed * 1e9 / ops, insert + find + mixed, bench::cpu_seconds());
    }
}

int main(int argc, char** argv) {
    std::string what = argc > 1 ? argv[1] : "";
    std::string v = argc > 2 ? argv[2] : "";
    int threads = argc > 3 ? std::atoi(argv[3]) : 4;
    if (what == "map" || what == "umap" || what == "set") {
        if (!bench::has_variant(v.c_str(), {"sgcl", "mutex", "rwlock"})) {
            std::fprintf(stderr, "usage: concurrent <map|umap|set> <sgcl|mutex|rwlock> [threads] [keys] [n]\n");
            return 2;
        }
        long keys = argc > 4 ? std::atol(argv[4]) : 200'000;
        long n = argc > 5 ? std::atol(argv[5]) : 200'000;
        if (what == "map") {
            if (v == "sgcl") {
                run_map<SgclMap>("map", threads, keys, n);
            } else if (v == "mutex") {
                run_map<MutexMap>("map", threads, keys, n);
            } else {
                run_map<RwlockMap>("map", threads, keys, n);
            }
        } else if (what == "umap") {
            if (v == "sgcl") {
                run_map<SgclUmap>("umap", threads, keys, n);
            } else if (v == "mutex") {
                run_map<Locked<std::unordered_map<long, std::shared_ptr<Item>>, std::mutex, Exclusive>>("umap", threads, keys, n);
            } else {
                run_map<Locked<std::unordered_map<long, std::shared_ptr<Item>>, std::shared_mutex, Shared>>("umap", threads, keys, n);
            }
        } else {
            if (v == "sgcl") {
                run_map<SgclSet>("set", threads, keys, n);
            } else if (v == "mutex") {
                run_map<Locked<std::set<long>, std::mutex, Exclusive>>("set", threads, keys, n);
            } else {
                run_map<Locked<std::set<long>, std::shared_mutex, Shared>>("set", threads, keys, n);
            }
        }
        return 0;
    }
    if (what == "chan") {
        if (!bench::has_variant(v.c_str(), {"sgcl", "mutex"})) {
            std::fprintf(stderr, "usage: concurrent chan <sgcl|mutex> [threads] [capacity] [n]\n");
            return 2;
        }
        size_t cap = argc > 4 ? (size_t)std::atol(argv[4]) : 64;
        long n = argc > 5 ? std::atol(argv[5]) : 200'000;
        if (v == "sgcl") {
            run_chan<SgclChan>(threads, cap, n);
        } else {
            run_chan<MutexChan>(threads, cap, n);
        }
        return 0;
    }
    if (what == "cow") {
        if (!bench::has_variant(v.c_str(), {"sgcl", "shared", "rwlock"})) {
            std::fprintf(stderr, "usage: concurrent cow <sgcl|shared|rwlock> [threads] [n]\n");
            return 2;
        }
        int t = argc > 3 ? std::atoi(argv[3]) : 16;
        long n = argc > 4 ? std::atol(argv[4]) : 2'000'000;
        if (v == "sgcl") {
            run_cow<SgclCow>(t, n);
        } else if (v == "shared") {
            run_cow<SharedCow>(t, n);
        } else {
            run_cow<RwlockCow>(t, n);
        }
        return 0;
    }
    std::string mode = argc > 4 ? argv[4] : "mixed";
    long n = argc > 5 ? std::atol(argv[5]) : 200'000;
    if ((what != "queue" && what != "stack") || !bench::has_variant(v.c_str(), {"sgcl", "mutex", "shared"})
        || (mode != "mixed" && mode != "pairs") || (mode == "pairs" && threads % 2)) {
        std::fprintf(stderr, "usage: concurrent <queue|stack> <sgcl|mutex|shared> [threads] [mixed|pairs (an even number of threads)] [n]\n"
                             "       concurrent map <sgcl|mutex|rwlock> [threads] [keys] [n]\n");
        return 2;
    }
    if (what == "queue") {
        if (v == "sgcl") {
            run_container<SgclQueue>("queue", threads, mode, n);
        } else if (v == "mutex") {
            run_container<MutexQueue>("queue", threads, mode, n);
        } else {
            run_container<SharedQueue>("queue", threads, mode, n);
        }
    } else {
        if (v == "sgcl") {
            run_container<SgclStack>("stack", threads, mode, n);
        } else if (v == "mutex") {
            run_container<MutexStack>("stack", threads, mode, n);
        } else {
            run_container<SharedStack>("stack", threads, mode, n);
        }
    }
    return 0;
}
