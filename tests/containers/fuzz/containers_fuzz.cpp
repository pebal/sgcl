//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The containers of core driven by a sequence of operations read from the
// input, each against a std container as its model: after every operation
// the size and the answer of the operation must be the model's, and every
// few operations (and at the end) the whole contents, in the order the
// container promises (sorted for sorted_*, insertion order for ordered_*,
// as a sorted multiset of elements for the hash containers). A copy made
// on the way must keep what it had while the original changes, and equal
// it again when given the same operations; swap exchanges two of them.
//
//   map, sorted_map, ordered_map, set, sorted_set, ordered_set,
//   multimap, multiset, sorted_multimap, sorted_multiset   against std::map /
//     std::set / std::multimap / std::multiset (ordered_*: the model's
//     insertion order kept beside it);
//   vector, deque, list                                     against std::deque.
//
// The input: the first byte picks the container, the rest is operations of
// three bytes (what, a key of 0..63 so that keys meet again, a value).
// Values of the sequences are sgcl::string, so that the elements are
// managed objects too.
//
// Built with libFuzzer (tests/fuzz/run.sh tests/containers/fuzz/containers_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/core/deque.h"
#include "sgcl/core/list.h"
#include "sgcl/core/map.h"
#include "sgcl/core/multimap.h"
#include "sgcl/core/multiset.h"
#include "sgcl/core/ordered_map.h"
#include "sgcl/core/ordered_set.h"
#include "sgcl/core/set.h"
#include "sgcl/core/sorted_map.h"
#include "sgcl/core/sorted_multimap.h"
#include "sgcl/core/sorted_multiset.h"
#include "sgcl/core/sorted_set.h"
#include "sgcl/core/string.h"
#include "sgcl/core/vector.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
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

    enum class Order { sorted, hashed, inserted };

    // --- maps and sets with unique keys -----------------------------------------------

    template<class C>
    constexpr bool is_map = requires { typename C::mapped_type; };

    // The model: a std::map (a set's mapped values unused) and, for
    // ordered_*, the keys in the order of insertion
    struct UniqueModel {
        std::map<int, int> m;
        std::vector<int> order;

        bool insert(int k, int v) {
            if (m.count(k)) {
                return false;
            }
            m[k] = v;
            order.push_back(k);
            return true;
        }

        size_t erase(int k) {
            if (!m.erase(k)) {
                return 0;
            }
            order.erase(std::find(order.begin(), order.end(), k));
            return 1;
        }

        void clear() {
            m.clear();
            order.clear();
        }
    };

    template<class C>
    int key_of(const typename C::value_type& e) {
        if constexpr (is_map<C>) {
            return e.first;
        } else {
            return e;
        }
    }

    template<class C>
    void same_unique(const C& c, const UniqueModel& model, Order order) {
        check(c.size() == model.m.size() && c.empty() == model.m.empty());
        std::vector<std::pair<int, int>> got;
        for (const auto& e : c) {
            if constexpr (is_map<C>) {
                got.emplace_back(e.first, e.second);
            } else {
                got.emplace_back(e, 0);
            }
        }
        check(got.size() == model.m.size());
        if (order == Order::inserted) {
            for (size_t i = 0; i < got.size(); ++i) {
                check(got[i].first == model.order[i]);
                if constexpr (is_map<C>) {
                    check(got[i].second == model.m.at(model.order[i]));
                }
            }
            return;
        }
        if (order == Order::hashed) {
            std::sort(got.begin(), got.end());
        }
        size_t i = 0;
        for (const auto& [k, v] : model.m) {
            check(got[i].first == k);
            if constexpr (is_map<C>) {
                check(got[i].second == v);
            }
            ++i;
        }
    }

    template<class C>
    void run_unique(const Op* ops, size_t n, Order order) {
        C c;
        UniqueModel model;
        std::optional<C> copy;
        UniqueModel copy_model;
        C other;
        UniqueModel other_model;
        for (size_t i = 0; i < n; ++i) {
            const int k = ops[i].key % 64;
            const int v = ops[i].value;
            switch (ops[i].what % 12) {
            case 0:
            case 1: {   // insert
                bool inserted;
                if constexpr (is_map<C>) {
                    inserted = c.insert({k, v}).second;
                } else {
                    inserted = c.insert(k).second;
                }
                check(inserted == model.insert(k, is_map<C> ? v : 0));
                break;
            }
            case 2:     // insert_or_assign / operator[] (a set: emplace)
                if constexpr (is_map<C>) {
                    if (v & 1) {
                        c[k] = v;
                    } else {
                        c.insert_or_assign(k, v);
                    }
                    if (!model.insert(k, v)) {
                        model.m[k] = v;
                    }
                } else {
                    check(c.emplace(k).second == model.insert(k, 0));
                }
                break;
            case 3:
            case 4:     // erase by key
                check(c.erase(k) == model.erase(k));
                break;
            case 5: {   // find, count, contains
                auto it = c.find(k);
                const bool here = model.m.count(k) != 0;
                check((it != c.end()) == here && c.count(k) == size_t(here) && c.contains(k) == here);
                if (here) {
                    check(key_of<C>(*it) == k);
                    if constexpr (is_map<C>) {
                        check(it->second == model.m.at(k));
                    }
                }
                break;
            }
            case 6: {   // erase through the iterator of find
                auto it = c.find(k);
                if (it != c.end()) {
                    c.erase(it);
                    model.erase(k);
                }
                break;
            }
            case 7:     // a copy, kept while the original changes
                copy.emplace(c);
                copy_model = model;
                break;
            case 8:     // swap with a second container
                c.swap(other);
                std::swap(model, other_model);
                break;
            case 9:     // erase the first element (the oldest of ordered_*)
                if (!c.empty()) {
                    const int first = key_of<C>(*c.begin());
                    c.erase(c.begin());
                    model.erase(first);
                }
                break;
            case 10:    // clear, rarely
                if (v < 16) {
                    c.clear();
                    model.clear();
                }
                break;
            default:    // the whole contents
                same_unique(c, model, order);
                break;
            }
            check(c.size() == model.m.size());
            if (copy) {
                check(copy->size() == copy_model.m.size());
            }
        }
        same_unique(c, model, order);
        same_unique(other, other_model, order);
        if (copy) {
            same_unique(*copy, copy_model, order);
            C assigned;
            assigned = *copy;
            same_unique(assigned, copy_model, order);
        }
    }

    // --- multimaps and multisets -----------------------------------------------------

    template<class C>
    void same_multi(const C& c, const std::multimap<int, int>& model, Order order) {
        check(c.size() == model.size());
        std::vector<std::pair<int, int>> got, want;
        for (const auto& e : c) {
            if constexpr (is_map<C>) {
                got.emplace_back(e.first, e.second);
            } else {
                got.emplace_back(e, 0);
            }
        }
        for (const auto& e : model) {
            want.push_back(e);
        }
        if (order == Order::hashed) {
            std::sort(got.begin(), got.end());
            std::sort(want.begin(), want.end());
            check(got == want);
        } else {
            // sorted by key; the values of one key in the order inserted, as std
            check(got == want);
        }
    }

    template<class C>
    void run_multi(const Op* ops, size_t n, Order order) {
        C c;
        std::multimap<int, int> model;
        for (size_t i = 0; i < n; ++i) {
            const int k = ops[i].key % 64;
            const int v = is_map<C> ? ops[i].value : 0;
            switch (ops[i].what % 6) {
            case 0:
            case 1:
                if constexpr (is_map<C>) {
                    c.insert({k, v});
                } else {
                    c.insert(k);
                }
                model.insert({k, v});
                break;
            case 2:
                check(c.erase(k) == model.erase(k));
                break;
            case 3: {
                check(c.count(k) == model.count(k));
                auto [a, b] = c.equal_range(k);
                check(size_t(std::distance(a, b)) == model.count(k));
                break;
            }
            case 4: {   // erase one element of the key
                auto it = c.find(k);
                if (it != c.end()) {
                    if constexpr (is_map<C>) {
                        const int value = it->second;
                        c.erase(it);
                        auto [a, b] = model.equal_range(k);
                        for (; a != b; ++a) {
                            if (a->second == value) {
                                model.erase(a);
                                break;
                            }
                        }
                    } else {
                        c.erase(it);
                        model.erase(model.find(k));
                    }
                }
                break;
            }
            default:
                same_multi(c, model, order);
                break;
            }
            check(c.size() == model.size());
        }
        same_multi(c, model, order);
        C copy(c);
        same_multi(copy, model, order);
    }

    // --- sequences ---------------------------------------------------------------------

    // The element of a model's value: -1 is a default one (an empty string)
    string text(int v) {
        return v < 0 ? string() : string(std::to_string(v * 7919));
    }

    template<class C>
    void same_sequence(const C& c, const std::deque<int>& model) {
        check(c.size() == model.size());
        size_t i = 0;
        for (const auto& e : c) {
            check(e == text(model[i]));
            ++i;
        }
        check(i == model.size());
        if (!model.empty()) {
            check(c.front() == text(model.front()) && c.back() == text(model.back()));
        }
    }

    template<class C>
    constexpr bool has_front = requires(C c) { c.push_front(string()); };

    template<class C>
    constexpr bool indexed = requires(C c) { c[0]; };

    template<class C>
    void run_sequence(const Op* ops, size_t n) {
        C c;
        std::deque<int> model;
        std::optional<C> copy;
        std::deque<int> copy_model;
        for (size_t i = 0; i < n; ++i) {
            const int v = ops[i].value | ops[i].key << 8;
            const size_t at = model.empty() ? 0 : size_t(ops[i].key) % (model.size() + 1);
            switch (ops[i].what % 11) {
            case 0:
            case 1:
                c.push_back(text(v));
                model.push_back(v);
                break;
            case 2:
                if constexpr (has_front<C>) {
                    c.push_front(text(v));
                    model.push_front(v);
                } else {
                    c.insert(c.begin(), text(v));
                    model.push_front(v);
                }
                break;
            case 3:
                if (!model.empty()) {
                    c.pop_back();
                    model.pop_back();
                }
                break;
            case 4:
                if (!model.empty()) {
                    if constexpr (has_front<C>) {
                        c.pop_front();
                    } else {
                        c.erase(c.begin());
                    }
                    model.pop_front();
                }
                break;
            case 5: {   // insert at a position
                auto it = c.begin();
                std::advance(it, at);
                c.insert(it, text(v));
                model.insert(model.begin() + at, v);
                break;
            }
            case 6:     // erase at a position
                if (!model.empty()) {
                    const size_t e = at % model.size();
                    auto it = c.begin();
                    std::advance(it, e);
                    c.erase(it);
                    model.erase(model.begin() + e);
                }
                break;
            case 7:     // index, or walk to it
                if (!model.empty()) {
                    const size_t e = at % model.size();
                    if constexpr (indexed<C>) {
                        check(c[e] == text(model[e]));
                        c[e] = text(v);
                    } else {
                        auto it = c.begin();
                        std::advance(it, e);
                        check(*it == text(model[e]));
                        *it = text(v);
                    }
                    model[e] = v;
                }
                break;
            case 8:     // resize
                if constexpr (requires { c.resize(size_t(0)); }) {
                    const size_t to = size_t(ops[i].key) % 80;
                    c.resize(to);
                    model.resize(to, -1);   // new elements: empty strings
                }
                break;
            case 9:
                copy.emplace(c);
                copy_model = model;
                break;
            default:
                same_sequence(c, model);
                break;
            }
            check(c.size() == model.size());
        }
        same_sequence(c, model);
        if (copy) {
            same_sequence(*copy, copy_model);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const Op* ops = reinterpret_cast<const Op*>(data + 1);
    static_assert(sizeof(Op) == 3);
    const size_t n = (size - 1) / 3;
    switch (data[0] % 13) {
    case 0: run_unique<map<int, int>>(ops, n, Order::hashed); break;
    case 1: run_unique<sorted_map<int, int>>(ops, n, Order::sorted); break;
    case 2: run_unique<ordered_map<int, int>>(ops, n, Order::inserted); break;
    case 3: run_unique<set<int>>(ops, n, Order::hashed); break;
    case 4: run_unique<sorted_set<int>>(ops, n, Order::sorted); break;
    case 5: run_unique<ordered_set<int>>(ops, n, Order::inserted); break;
    case 6: run_multi<multimap<int, int>>(ops, n, Order::hashed); break;
    case 7: run_multi<multiset<int>>(ops, n, Order::hashed); break;
    case 8: run_multi<sorted_multimap<int, int>>(ops, n, Order::sorted); break;
    case 9: run_multi<sorted_multiset<int>>(ops, n, Order::sorted); break;
    case 10: run_sequence<vector<string>>(ops, n); break;
    case 11: run_sequence<deque<string>>(ops, n); break;
    default: run_sequence<list<string>>(ops, n); break;
    }
    return 0;
}
