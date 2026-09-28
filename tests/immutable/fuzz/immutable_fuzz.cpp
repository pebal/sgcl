//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The immutable containers driven by a sequence of operations read from the
// input, against std models: every operation makes a new version, which
// must hold what the model says; the versions kept on the way (up to eight,
// each with a copy of its model) must hold exactly what they held when
// they were made, after every later operation: one of them compared whole
// after each operation, all of them at the end. The builders (thaw,
// changes in place, freeze) go through the same model, and the version
// thawed must not change under them.
//
//   vector<int>                    push_back, pop_back, set, at, iteration
//   map<int, int>, set<int>        insert, set, erase, find, try_get, the builder;
//                                  also with a hash of seven values, so that
//                                  keys meet in one entry and in chains
//   list<int>                      push_front, pop_front, reverse
//
// The input: the first byte picks the container, the rest is operations of
// three bytes (what, a key or position, a value).
//
// Built with libFuzzer (tests/fuzz/run.sh tests/immutable/fuzz/immutable_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/core/vector.h"
#include "sgcl/immutable/immutable.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
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

    // A hash of seven values: most keys share an entry, and those whose
    // hashes agree to the last bit hang in a chain
    struct Weak {
        size_t operator()(int k) const noexcept {
            return size_t(k % 7);
        }
    };

    // Up to eight versions kept with their models; the oldest replaced. In
    // an sgcl::vector: a version holds tracked words, which a std
    // container's memory would hide from the collector (Rule 1); the models
    // beside them are unmanaged data inside a managed element
    template<class V, class M>
    struct Kept {
        sgcl::vector<std::pair<V, M>> all;
        size_t next = 0;
        size_t turn = 0;

        void keep(const V& v, const M& m) {
            if (all.size() < 8) {
                all.emplace_back(v, m);
            } else {
                all[next++ % 8] = {v, m};
            }
        }

        template<class Same>
        void one(Same same) {
            if (!all.empty()) {
                auto& [v, m] = all[turn++ % all.size()];
                same(v, m);
            }
        }

        template<class Same>
        void every(Same same) {
            for (auto& [v, m] : all) {
                same(v, m);
            }
        }
    };

    // --- vector ---------------------------------------------------------------------

    void same_vector(const immutable::vector<int>& v, const std::vector<int>& m) {
        check(v.size() == m.size() && v.empty() == m.empty());
        size_t i = 0;
        for (int x : v) {
            check(x == m[i++]);
        }
        check(i == m.size());
        if (!m.empty()) {
            check(v.front() == m.front() && v.back() == m.back());
            check(v[m.size() / 2] == m[m.size() / 2] && v.at(m.size() - 1) == m.back());
        }
    }

    void run_vector(const Op* ops, size_t n) {
        immutable::vector<int> v;
        std::vector<int> m;
        Kept<immutable::vector<int>, std::vector<int>> kept;
        for (size_t i = 0; i < n; ++i) {
            const int x = ops[i].value | ops[i].key << 8;
            switch (ops[i].what % 7) {
            case 0:
            case 1:
            case 2: {   // push_back, many at once now and then (the trie grows a level)
                const int times = ops[i].what % 7 == 2 ? int(ops[i].key) * 4 + 1 : 1;
                for (int t = 0; t < times; ++t) {
                    v = v.push_back(x + t);
                    m.push_back(x + t);
                }
                break;
            }
            case 3:
                if (!m.empty()) {
                    const int times = std::min<int>(int(ops[i].key % 40) + 1, int(m.size()));
                    for (int t = 0; t < times; ++t) {
                        v = v.pop_back();
                        m.pop_back();
                    }
                }
                break;
            case 4:
                if (!m.empty()) {
                    const size_t at = (size_t(ops[i].key) * 131 + ops[i].value) % m.size();
                    v = v.set(at, x);
                    m[at] = x;
                }
                break;
            case 5:
                kept.keep(v, m);
                break;
            default:
                same_vector(v, m);
                break;
            }
            check(v.size() == m.size());
            kept.one(same_vector);
        }
        same_vector(v, m);
        kept.every(same_vector);
    }

    // --- map and set -----------------------------------------------------------------

    template<class C>
    constexpr bool is_map = requires { typename C::mapped_type; };

    template<class C>
    void same_assoc(const C& c, const std::map<int, int>& m) {
        check(c.size() == m.size() && c.empty() == m.empty());
        std::vector<std::pair<int, int>> got;
        for (const auto& e : c) {
            if constexpr (is_map<C>) {
                got.emplace_back(e.first, e.second);
            } else {
                got.emplace_back(e, 0);
            }
        }
        std::sort(got.begin(), got.end());
        check(got.size() == m.size() && std::equal(got.begin(), got.end(), m.begin(), [](const auto& a, const auto& b) {
            return a.first == b.first && a.second == b.second;
        }));
    }

    template<class C>
    void run_assoc(const Op* ops, size_t n) {
        C c;
        std::map<int, int> m;
        Kept<C, std::map<int, int>> kept;
        for (size_t i = 0; i < n; ++i) {
            const int k = ops[i].key % 64;
            const int x = is_map<C> ? ops[i].value : 0;
            switch (ops[i].what % 8) {
            case 0:
            case 1:     // insert: keeps what it finds
                if constexpr (is_map<C>) {
                    c = c.insert(k, x);
                } else {
                    c = c.insert(k);
                }
                m.emplace(k, x);
                break;
            case 2:     // set (a set: insert again)
                if constexpr (is_map<C>) {
                    c = c.set(k, x);
                    m[k] = x;
                } else {
                    c = c.insert(k);
                    m.emplace(k, 0);
                }
                break;
            case 3:
                c = c.erase(k);
                m.erase(k);
                break;
            case 4: {   // find, contains, count, try_get
                const bool here = m.count(k) != 0;
                auto it = c.find(k);
                check((it != c.end()) == here && c.contains(k) == here && c.count(k) == size_t(here));
                if constexpr (is_map<C>) {
                    const int* p = c.try_get(k);
                    check((p != nullptr) == here);
                    if (here) {
                        check(*p == m.at(k) && it->second == m.at(k) && c.at(k) == m.at(k));
                    }
                }
                break;
            }
            case 5: {   // the builder: a few changes in place, then frozen
                auto before = c;
                auto before_model = m;
                auto b = c.thaw();
                const int count = ops[i].value % 16 + 1;
                for (int t = 0; t < count; ++t) {
                    const int kk = (k + t * 5) % 64;
                    if ((t + ops[i].value) % 3 == 2) {
                        check(b.erase(kk) == (m.erase(kk) != 0));
                    } else if constexpr (is_map<C>) {
                        const bool added = b.set(kk, x + t);
                        check(added == (m.count(kk) == 0));
                        m[kk] = x + t;
                    } else {
                        check(b.insert(kk) == m.emplace(kk, 0).second);
                    }
                    check(b.size() == m.size() && b.contains(kk) == (m.count(kk) != 0));
                }
                c = b.freeze();
                same_assoc(before, before_model);   // the version thawed is unchanged
                break;
            }
            case 6:
                kept.keep(c, m);
                break;
            default:
                same_assoc(c, m);
                break;
            }
            check(c.size() == m.size());
            kept.one(same_assoc<C>);
        }
        same_assoc(c, m);
        kept.every(same_assoc<C>);
    }

    // --- list ------------------------------------------------------------------------

    void same_list(const immutable::list<int>& l, const std::deque<int>& m) {
        check(l.size() == m.size() && l.empty() == m.empty());
        size_t i = 0;
        for (int x : l) {
            check(x == m[i++]);
        }
        check(i == m.size());
        if (!m.empty()) {
            check(l.front() == m.front());
        }
    }

    void run_list(const Op* ops, size_t n) {
        immutable::list<int> l;
        std::deque<int> m;
        Kept<immutable::list<int>, std::deque<int>> kept;
        for (size_t i = 0; i < n; ++i) {
            const int x = ops[i].value | ops[i].key << 8;
            switch (ops[i].what % 6) {
            case 0:
            case 1:
                l = l.push_front(x);
                m.push_front(x);
                break;
            case 2:
                if (!m.empty()) {
                    l = l.pop_front();
                    m.pop_front();
                }
                break;
            case 3:
                l = l.reverse();
                std::reverse(m.begin(), m.end());
                break;
            case 4:
                kept.keep(l, m);
                break;
            default:
                same_list(l, m);
                break;
            }
            check(l.size() == m.size());
            kept.one(same_list);
        }
        same_list(l, m);
        kept.every(same_list);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    static_assert(sizeof(Op) == 3);
    const Op* ops = reinterpret_cast<const Op*>(data + 1);
    const size_t n = (size - 1) / 3;
    switch (data[0] % 6) {
    case 0: run_vector(ops, n); break;
    case 1: run_assoc<immutable::map<int, int>>(ops, n); break;
    case 2: run_assoc<immutable::set<int>>(ops, n); break;
    case 3: run_assoc<immutable::map<int, int, Weak>>(ops, n); break;
    case 4: run_assoc<immutable::set<int, Weak>>(ops, n); break;
    default: run_list(ops, n); break;
    }
    return 0;
}
