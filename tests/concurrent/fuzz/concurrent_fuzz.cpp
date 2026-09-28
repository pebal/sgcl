//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The concurrent structures driven by a sequence of operations read from
// the input, on one thread, against std models: what one thread sees of
// them must be what the sequential structure would show. Concurrency itself
// is the TSan runs' and the stress tests' (tests/concurrent); this holds
// the sequential meaning of every operation, the paths a race is never
// needed to reach (a bucket split, a node folded, an empty queue refilled).
//
//   map<int, int>, sorted_map<int, int>, set<int>, sorted_set<int>
//       insert, try_emplace, erase (by key and by iterator), find, value_or,
//       lower_bound, iteration, clear   against std::map / std::set
//   queue<int>, stack<int>              push, try_pop, clear   against std::deque
//   priority_queue<int>                 push, try_pop, try_top   against std::multiset
//   copy_on_write<std::vector<int>>     store, update, load, compare_exchange;
//       every snapshot taken must keep what it saw
//
// The input: the first byte picks the structure, the rest is operations of
// three bytes (what, a key, a value).
//
// Built with libFuzzer (tests/fuzz/run.sh tests/concurrent/fuzz/concurrent_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/concurrent/copy_on_write.h"
#include "sgcl/concurrent/map.h"
#include "sgcl/concurrent/priority_queue.h"
#include "sgcl/concurrent/queue.h"
#include "sgcl/concurrent/set.h"
#include "sgcl/concurrent/sorted_map.h"
#include "sgcl/concurrent/sorted_set.h"
#include "sgcl/concurrent/stack.h"
#include "sgcl/core/vector.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Op {
        uint8_t what, key, value;
    };

    template<class C>
    constexpr bool is_map = requires { typename C::mapped_type; };

    template<class C>
    void same_assoc(C& c, const std::map<int, int>& m, bool sorted) {
        check(c.size() == m.size() && c.empty() == m.empty());
        std::vector<std::pair<int, int>> got;
        for (auto it = c.begin(); it != c.end(); ++it) {
            if constexpr (is_map<C>) {
                got.emplace_back(it->first, it->second);
            } else {
                got.emplace_back(*it, 0);
            }
        }
        if (!sorted) {
            std::sort(got.begin(), got.end());
        }
        check(got.size() == m.size() && std::equal(got.begin(), got.end(), m.begin(), [](const auto& a, const auto& b) {
            return a.first == b.first && a.second == b.second;
        }));
    }

    template<class C>
    void run_assoc(const Op* ops, size_t n, bool sorted) {
        C c;
        std::map<int, int> m;
        for (size_t i = 0; i < n; ++i) {
            const int k = ops[i].key % 128;
            const int x = is_map<C> ? ops[i].value : 0;
            switch (ops[i].what % 10) {
            case 0:
            case 1: {
                bool added;
                if constexpr (is_map<C>) {
                    added = c.insert({k, x}).second;
                } else {
                    added = c.insert(k).second;
                }
                check(added == m.emplace(k, x).second);
                break;
            }
            case 2:
                if constexpr (is_map<C>) {
                    check(c.try_emplace(k, x).second == m.try_emplace(k, x).second);
                } else {
                    check(c.emplace(k).second == m.emplace(k, 0).second);
                }
                break;
            case 3:
            case 4:
                check(c.erase(k) == m.erase(k));
                break;
            case 5: {
                auto it = c.find(k);
                const bool here = m.count(k) != 0;
                check((it != c.end()) == here && c.contains(k) == here && c.count(k) == size_t(here));
                if constexpr (is_map<C>) {
                    check(c.value_or(k, -1) == (here ? m.at(k) : -1));
                    if (here) {
                        check(it->second == m.at(k));
                    }
                }
                break;
            }
            case 6: {   // erase through an iterator
                auto it = c.find(k);
                if (it != c.end()) {
                    c.erase(it);
                    m.erase(k);
                }
                break;
            }
            case 7:
                if constexpr (requires { c.lower_bound(k); }) {
                    auto it = c.lower_bound(k);
                    auto want = m.lower_bound(k);
                    check((it == c.end()) == (want == m.end()));
                    if (want != m.end()) {
                        if constexpr (is_map<C>) {
                            check(it->first == want->first);
                        } else {
                            check(*it == want->first);
                        }
                    }
                }
                break;
            case 8:
                if (x < 8 || (!is_map<C> && ops[i].value < 8)) {
                    c.clear();
                    m.clear();
                }
                break;
            default:
                same_assoc(c, m, sorted);
                break;
            }
            check(c.size() == m.size());
        }
        same_assoc(c, m, sorted);
    }

    template<class C, bool Fifo>
    void run_sequence(const Op* ops, size_t n) {
        C c;
        std::deque<int> m;
        for (size_t i = 0; i < n; ++i) {
            const int x = ops[i].value | ops[i].key << 8;
            switch (ops[i].what % 5) {
            case 0:
            case 1: {   // push, a burst now and then
                const int times = ops[i].what % 5 == 1 ? ops[i].key % 70 + 1 : 1;
                for (int t = 0; t < times; ++t) {
                    c.push(x + t);
                    m.push_back(x + t);
                }
                break;
            }
            case 2:
            case 3: {
                const int times = ops[i].what % 5 == 3 ? ops[i].key % 70 + 1 : 1;
                for (int t = 0; t < times; ++t) {
                    auto got = c.try_pop();
                    check(bool(got) == !m.empty());
                    if (got) {
                        if (Fifo) {
                            check(*got == m.front());
                            m.pop_front();
                        } else {
                            check(*got == m.back());
                            m.pop_back();
                        }
                    }
                }
                break;
            }
            default:
                if (ops[i].value < 16) {
                    c.clear();
                    m.clear();
                }
                break;
            }
            check(c.size() == m.size() && c.empty() == m.empty());
        }
        while (auto got = c.try_pop()) {
            check(!m.empty() && *got == (Fifo ? m.front() : m.back()));
            Fifo ? m.pop_front() : m.pop_back();
        }
        check(m.empty() && c.empty());
    }

    void run_priority(const Op* ops, size_t n) {
        concurrent::priority_queue<int> c;
        std::multiset<int> m;
        for (size_t i = 0; i < n; ++i) {
            const int x = ops[i].value | (ops[i].key % 4) << 8;
            switch (ops[i].what % 5) {
            case 0:
            case 1:
                c.push(x);
                m.insert(x);
                break;
            case 2: {
                auto got = c.try_pop();
                check(bool(got) == !m.empty());
                if (got) {
                    check(*got == *m.begin());
                    m.erase(m.begin());
                }
                break;
            }
            case 3: {
                auto top = c.try_top();
                check(bool(top) == !m.empty() && (!top || *top == *m.begin()));
                break;
            }
            default:
                if (ops[i].value < 16) {
                    c.clear();
                    m.clear();
                }
                break;
            }
            check(c.size() == m.size());
        }
        while (auto got = c.try_pop()) {
            check(!m.empty() && *got == *m.begin());
            m.erase(m.begin());
        }
        check(m.empty());
    }

    void run_cow(const Op* ops, size_t n) {
        using V = std::vector<int>;
        concurrent::copy_on_write<V> c;
        V m;
        // every snapshot taken with what it saw; in an sgcl::vector, a
        // snapshot being a tracked word (Rule 1)
        sgcl::vector<concurrent::copy_on_write<V>::snapshot> snaps;
        std::vector<V> seen;
        for (size_t i = 0; i < n; ++i) {
            const int x = ops[i].value | ops[i].key << 8;
            switch (ops[i].what % 5) {
            case 0: {
                V v(size_t(ops[i].key % 9), x);
                c.store(v);
                m = v;
                break;
            }
            case 1: {
                auto after = c.update([x](V& v) {
                    v.push_back(x);
                });
                m.push_back(x);
                check(*after == m);
                break;
            }
            case 2:
                if (snaps.size() < 16) {
                    snaps.push_back(c.load());
                    seen.push_back(m);
                }
                break;
            case 3: {   // compare_exchange: from a stale snapshot it fails, from the current one it succeeds
                auto expected = snaps.empty() ? c.load() : snaps[ops[i].key % snaps.size()];
                const bool current = expected.get() == c.load().get();
                V desired{x};
                const bool done = c.compare_exchange(expected, desired);
                check(done == current);
                if (done) {
                    m = desired;
                } else {
                    check(*expected == m);   // the current value handed back
                }
                break;
            }
            default:
                check(*c.load() == m);
                break;
            }
            check(*c.load() == m);
        }
        for (size_t i = 0; i < snaps.size(); ++i) {
            check(*snaps[i] == seen[i]);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    static_assert(sizeof(Op) == 3);
    const Op* ops = reinterpret_cast<const Op*>(data + 1);
    const size_t n = (size - 1) / 3;
    switch (data[0] % 8) {
    case 0: run_assoc<concurrent::map<int, int>>(ops, n, false); break;
    case 1: run_assoc<concurrent::sorted_map<int, int>>(ops, n, true); break;
    case 2: run_assoc<concurrent::set<int>>(ops, n, false); break;
    case 3: run_assoc<concurrent::sorted_set<int>>(ops, n, true); break;
    case 4: run_sequence<concurrent::queue<int>, true>(ops, n); break;
    case 5: run_sequence<concurrent::stack<int>, false>(ops, n); break;
    case 6: run_priority(ops, n); break;
    default: run_cow(ops, n); break;
    }
    return 0;
}
