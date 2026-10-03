//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of core's maps and sets (DESIGN 356, 408): the hash
// tables (map, multimap, set, multiset, ordered_map, ordered_set) and the
// trees (sorted_*). Every operation that makes, copies or moves an element
// run with a throw at each of its points in turn (tests/containers/
// exception_safety.h), on containers of 0, 1 and 9 elements (several to a
// chain: the hash is the int mod 4), with a key that is there and one that
// is not, and checked against what its page says in Exceptions: nothing
// inserted and the container as it was where the page says so, else every
// element alive accounted for and none read after its end.
#include "tests/containers/exception_safety.h"

#include <algorithm>
#include <iterator>

using es::Element;
using es::NothrowElement;

namespace {
    using Content = std::multimap<int, int>;

    template<class C>
    inline constexpr bool is_map = requires { typename C::mapped_type; };

    // A container of n elements: keys 0..n-1 (a map's value ten times the
    // key), the arguments (a new key -1 and an old key n / 2, a value -10,
    // the elements of either key with that value, a source of four new
    // elements 100..103, another container of seven 200..206) and what was
    // there before
    template<class C>
    struct AssocState {
        using K = typename C::key_type;
        using V = typename C::value_type;

        int base = int(K::alive);
        C c;
        std::optional<K> fresh;
        std::optional<K> old;
        std::optional<K> value;
        std::optional<V> fresh_element;
        std::optional<V> old_element;
        std::vector<V> src;
        std::optional<C> other;
        Content before;
        int n;

        explicit AssocState(int n)
        : n(n) {
            for (int i = 0; i < n; ++i) {
                add(c, i);
            }
            fresh.emplace(-1);
            old.emplace(n / 2);
            value.emplace(-10);
            fresh_element.emplace(make(-1, -10));
            old_element.emplace(make(n / 2, -10));
            src.reserve(4);
            for (int k = 0; k < 4; ++k) {
                src.push_back(make(100 + k, (100 + k) * 10));
            }
            other.emplace();
            for (int k = 0; k < 7; ++k) {
                add(*other, 200 + k);
            }
            before = es::content(c);
        }

        static V make(int k, int v) {
            if constexpr (is_map<C>) {
                return V(K(k), K(v));
            } else {
                (void)v;
                return V(k);
            }
        }

        static void add(C& to, int k) {
            if constexpr (is_map<C>) {
                to.emplace(k, k * 10);
            } else {
                to.emplace(k);
            }
        }

        // The objects of an element: two for a map's (key and value)
        static constexpr int per = is_map<C> ? 2 : 1;

        int extra() const {
            return (fresh ? 1 : 0) + (old ? 1 : 0) + (value ? 1 : 0) + (fresh_element ? per : 0) + (old_element ? per : 0)
                + per * int(src.size()) + (other ? per * int(other->size()) : 0);
        }

        void accounted() const {
            auto walked = int(std::distance(c.begin(), c.end()));
            EXPECT_EQ(walked, int(c.size()));
            EXPECT_EQ(int(K::alive), base + per * walked + extra());
        }

        void reset() {
            c.clear();
            fresh.reset();
            old.reset();
            value.reset();
            fresh_element.reset();
            old_element.reset();
            src.clear();
            other.reset();
        }
    };

    // A content as its pairs in order: equal keys of a multi container
    // compared whatever the order they came in
    std::vector<std::pair<int, int>> pairs(const Content& c) {
        std::vector<std::pair<int, int>> out(c.begin(), c.end());
        std::sort(out.begin(), out.end());
        return out;
    }

    template<class C, class Op, class Expected, class Strong>
    void check_assoc(const std::string& what, int n, Op op, Expected expected, Strong strong) {
        es::each_throw(what + " of " + std::to_string(n), [=] { return sgcl::make_tracked<AssocState<C>>(n); }, op,
            [&](AssocState<C>& s, bool threw, unsigned kind) {
                s.accounted();
                auto now = es::content(s.c);
                if (threw) {
                    if (strong(s, kind)) {
                        EXPECT_EQ(pairs(now), pairs(s.before)) << "the page promises the container as it was";
                    }
                } else {
                    EXPECT_EQ(pairs(now), pairs(expected(s)));
                }
            });
    }

    const auto always = [](const auto&, unsigned) { return true; };
    const auto never = [](const auto&, unsigned) { return false; };

    // Whether the container keeps elements of equal keys (a multi one)
    template<class C>
    inline constexpr bool is_multi = requires(C& c, const typename C::value_type& e) {
        { c.insert(e) } -> std::same_as<typename C::iterator>;
    };

    // The content after the insertion of (k, v) (a set's k): a unique
    // container keeps what it has under k, a multi one adds it
    template<class C>
    Content inserted(const AssocState<C>& s, int k, int v) {
        Content out = s.before;
        if (is_multi<C> || !out.count(k)) {
            out.emplace(k, is_map<C> ? v : 0);
        }
        return out;
    }

    // The content with the value under k replaced (insert_or_assign,
    // operator[] of a new key)
    Content assigned(Content out, int k, int v) {
        out.erase(k);
        out.emplace(k, v);
        return out;
    }

    Content of_source() {
        Content out;
        for (int k = 100; k < 104; ++k) {
            out.emplace(k, 0);
        }
        return out;
    }

    template<class C>
    void operations(const std::string& name) {
        using K = typename C::key_type;
        for (int n : {0, 1, 9}) {
            for (bool there : {false, true}) {
                if (there && !n) {
                    continue;
                }
                const std::string which = there ? " of a key there" : " of a new key";
                auto k_of = [there](const auto& s) { return there ? s.n / 2 : -1; };
                auto element = [there](auto& s) -> auto& { return there ? *s.old_element : *s.fresh_element; };
                auto key = [there](auto& s) -> K& { return there ? *s.old : *s.fresh; };
                auto added = [k_of](auto& s) { return inserted(s, k_of(s), -10); };
                check_assoc<C>(name + ": insert(const value_type&)" + which, n, [element](auto& s) { s.c.insert(std::as_const(element(s))); }, added, always);
                check_assoc<C>(name + ": insert(value_type&&)" + which, n, [element](auto& s) { s.c.insert(std::move(element(s))); }, added, always);
                check_assoc<C>(name + ": insert(hint, const value_type&)" + which, n, [element](auto& s) { s.c.insert(s.c.begin(), std::as_const(element(s))); }, added, always);
                if constexpr (is_map<C>) {
                    check_assoc<C>(name + ": emplace(key, value)" + which, n, [key](auto& s) { s.c.emplace(key(s), *s.value); }, added, always);
                    check_assoc<C>(name + ": emplace_hint(end, key, value)" + which, n, [key](auto& s) { s.c.emplace_hint(s.c.end(), key(s), *s.value); }, added, always);
                    check_assoc<C>(name + ": emplace(int, int)" + which, n, [k_of](auto& s) { s.c.emplace(k_of(s), -10); }, added, always);
                    if constexpr (requires(C& c, const K& k) { c.try_emplace(k, k); }) {
                        check_assoc<C>(name + ": try_emplace(const Key&, value)" + which, n, [key](auto& s) { s.c.try_emplace(std::as_const(key(s)), *s.value); }, added, always);
                        check_assoc<C>(name + ": try_emplace(Key&&, value)" + which, n, [key](auto& s) { s.c.try_emplace(std::move(key(s)), *s.value); }, added, always);
                        check_assoc<C>(name + ": operator[](const Key&)" + which, n, [key](auto& s) { s.c[std::as_const(key(s))]; },
                            [there, k_of](auto& s) { return there ? s.before : inserted(s, k_of(s), 0); }, always);
                        check_assoc<C>(name + ": operator[](Key&&)" + which, n, [key](auto& s) { s.c[std::move(key(s))]; },
                            [there, k_of](auto& s) { return there ? s.before : inserted(s, k_of(s), 0); }, always);
                        // a new key is inserted (strong); an old one's value
                        // assigned: what the assignment of T leaves
                        check_assoc<C>(name + ": insert_or_assign(const Key&, const T&)" + which, n, [key](auto& s) { s.c.insert_or_assign(std::as_const(key(s)), std::as_const(*s.value)); },
                            [k_of](auto& s) { return assigned(s.before, k_of(s), -10); }, [there](const auto&, unsigned) { return !there; });
                        check_assoc<C>(name + ": insert_or_assign(Key&&, T&&)" + which, n, [key](auto& s) { s.c.insert_or_assign(std::move(key(s)), std::move(*s.value)); },
                            [k_of](auto& s) { return assigned(s.before, k_of(s), -10); }, [there](const auto&, unsigned) { return !there; });
                        if (there) {
                            check_assoc<C>(name + ": take" + which, n, [key](auto& s) { (void)s.c.take(key(s)); },
                                [k_of](auto& s) { Content out = s.before; out.erase(k_of(s)); return out; }, always);
                        }
                    }
                } else {
                    check_assoc<C>(name + ": emplace(int)" + which, n, [k_of](auto& s) { s.c.emplace(k_of(s)); }, added, always);
                    check_assoc<C>(name + ": emplace_hint(end, int)" + which, n, [k_of](auto& s) { s.c.emplace_hint(s.c.end(), k_of(s)); }, added, always);
                }
            }
            // the range keeps the elements inserted before the one that threw
            check_assoc<C>(name + ": insert(first, last)", n, [](auto& s) { s.c.insert(s.src.begin(), s.src.end()); },
                [](auto& s) {
                    Content out = s.before;
                    for (int k = 100; k < 104; ++k) {
                        out.emplace(k, is_map<C> ? k * 10 : 0);
                    }
                    return out;
                }, never);
            // a hash table builds the copy aside (strong); a tree is left
            // empty by a copy that throws
            check_assoc<C>(name + ": operator=(const&)", n, [](auto& s) { s.c = *s.other; },
                [](auto& s) { return es::content(*s.other); }, [](const auto&, unsigned) { return !requires { typename C::key_compare; }; });
            check_assoc<C>(name + ": copy constructor", n, [](auto& s) { C copy(*s.other); s.c.swap(copy); },
                [](auto& s) { return es::content(*s.other); }, always);
            check_assoc<C>(name + ": range constructor", n, [](auto& s) { C made(s.src.begin(), s.src.end()); s.c.swap(made); },
                [](auto&) {
                    Content out;
                    for (int k = 100; k < 104; ++k) {
                        out.emplace(k, is_map<C> ? k * 10 : 0);
                    }
                    return out;
                }, always);
        }
    }
}

TEST(ExceptionSafety_Test, HashTables) {
    operations<sgcl::map<Element, Element, es::Hash>>("map");
    operations<sgcl::multimap<Element, Element, es::Hash>>("multimap");
    operations<sgcl::set<Element, es::Hash>>("set");
    operations<sgcl::multiset<Element, es::Hash>>("multiset");
    operations<sgcl::map<NothrowElement, NothrowElement, es::Hash>>("map, a move that cannot throw");
}

TEST(ExceptionSafety_Test, OrderedHashTables) {
    operations<sgcl::ordered_map<Element, Element, es::Hash>>("ordered_map");
    operations<sgcl::ordered_set<Element, es::Hash>>("ordered_set");
}

TEST(ExceptionSafety_Test, Trees) {
    operations<sgcl::sorted_map<Element, Element>>("sorted_map");
    operations<sgcl::sorted_multimap<Element, Element>>("sorted_multimap");
    operations<sgcl::sorted_set<Element>>("sorted_set");
    operations<sgcl::sorted_multiset<Element>>("sorted_multiset");
}

namespace {
    std::vector<int> concat_one(std::vector<int> v) {
        v.push_back(-1);
        return v;
    }

    std::vector<int> iota7() {
        return {200, 201, 202, 203, 204, 205, 206};
    }

    const auto always_strong = [](const auto&, unsigned) { return true; };
    const auto never_strong = [](const auto&, unsigned) { return false; };

    // An adapter whose container the test reads (the adapters keep it as
    // the protected member c, as std's)
    template<class A>
    struct Opened : A {
        using A::A;
        using A::c;
    };

    // An adapter of n elements 0..n-1 pushed in order, the arguments (an
    // element -1, a source of four 100..103, another adapter of seven
    // 200..206) and its elements before, sorted for a priority_queue
    template<class A>
    struct AdapterState {
        using T = typename A::value_type;

        int base = int(T::alive);
        std::optional<Opened<A>> held;
        Opened<A>& a = held.emplace();
        std::optional<T> arg;
        std::vector<T> src;
        std::optional<Opened<A>> other;
        std::vector<int> before;

        explicit AdapterState(int n) {
            for (int i = 0; i < n; ++i) {
                a.emplace(i);
            }
            arg.emplace(-1);
            src.reserve(4);
            for (int k = 0; k < 4; ++k) {
                src.emplace_back(100 + k);
            }
            other.emplace();
            for (int k = 0; k < 7; ++k) {
                other->emplace(200 + k);
            }
            before = elements();
        }

        // The elements in the container's order; a priority_queue's sorted
        std::vector<int> elements() const {
            auto out = es::ints(a.c);
            if constexpr (requires { typename A::value_compare; }) {
                std::sort(out.begin(), out.end());
            }
            return out;
        }

        int extra() const {
            return (arg ? 1 : 0) + int(src.size()) + (other ? int(other->size()) : 0);
        }

        void accounted() const {
            EXPECT_EQ(int(std::distance(a.c.begin(), a.c.end())), int(a.size()));
            EXPECT_EQ(int(T::alive), base + int(a.size()) + extra());
        }

        void reset() {
            held.reset();
            arg.reset();
            src.clear();
            other.reset();
        }
    };

    template<class A, class Op, class Expected, class Strong>
    void check_adapter(const std::string& what, int n, Op op, Expected expected, Strong strong) {
        es::each_throw(what + " of " + std::to_string(n), [=] { return sgcl::make_tracked<AdapterState<A>>(n); }, op,
            [&](AdapterState<A>& s, bool threw, unsigned kind) {
                s.accounted();
                auto now = s.elements();
                if (threw) {
                    if (strong(s, kind)) {
                        EXPECT_EQ(now, s.before) << "the page promises the adapter as it was";
                    }
                } else {
                    EXPECT_EQ(now, expected(s));
                }
            });
    }

    std::vector<int> sorted(std::vector<int> v) {
        std::sort(v.begin(), v.end());
        return v;
    }

    // queue and stack: a push or an emplace leaves the adapter as it was
    // (their containers' push_back and emplace_back are strong); the
    // constructors leave nothing alive; the copy assignment is the
    // container's
    template<class A>
    void queue_or_stack(const std::string& name) {
        for (int n : {0, 1, 5}) {
            check_adapter<A>(name + ": push(const T&)", n, [](auto& s) { s.a.push(std::as_const(*s.arg)); }, [](auto& s) { return concat_one(s.before); }, always_strong);
            check_adapter<A>(name + ": push(T&&)", n, [](auto& s) { s.a.push(std::move(*s.arg)); }, [](auto& s) { return concat_one(s.before); }, always_strong);
            check_adapter<A>(name + ": emplace", n, [](auto& s) { s.a.emplace(-1); }, [](auto& s) { return concat_one(s.before); }, always_strong);
            check_adapter<A>(name + ": operator=(const&)", n, [](auto& s) { s.a = *s.other; }, [](auto&) { return iota7(); }, never_strong);
            check_adapter<A>(name + ": copy constructor", n, [](auto& s) { Opened<A> copy(*s.other); s.a.swap(copy); }, [](auto&) { return iota7(); }, always_strong);
            check_adapter<A>(name + ": constructor from a range", n, [](auto& s) { Opened<A> made(s.src.begin(), s.src.end()); s.a.swap(made); },
                [](auto&) { return std::vector<int>{100, 101, 102, 103}; }, always_strong);
        }
    }

    // priority_queue: a push or an emplace whose element's construction
    // throws leaves it as it was; a move of the sift, of pop or of the heap
    // made by a constructor leaves the elements valid (basic)
    template<class A>
    void priority_queue_of(const std::string& name) {
        const auto made_strong = [](const auto&, unsigned kind) { return kind != throwing::Move; };
        for (int n : {0, 1, 5, 20}) {
            check_adapter<A>(name + ": push(const T&)", n, [](auto& s) { s.a.push(std::as_const(*s.arg)); }, [](auto& s) { return sorted(concat_one(s.before)); }, made_strong);
            check_adapter<A>(name + ": push(T&&)", n, [](auto& s) { s.a.push(std::move(*s.arg)); }, [](auto& s) { return sorted(concat_one(s.before)); }, made_strong);
            check_adapter<A>(name + ": emplace", n, [](auto& s) { s.a.emplace(-1); }, [](auto& s) { return sorted(concat_one(s.before)); }, made_strong);
            if (n) {
                check_adapter<A>(name + ": pop", n, [](auto& s) { s.a.pop(); }, [](auto& s) { auto v = s.before; v.pop_back(); return v; }, never_strong);
            }
            check_adapter<A>(name + ": constructor from a range", n, [](auto& s) { Opened<A> made(s.src.begin(), s.src.end()); s.a.swap(made); },
                [](auto&) { return std::vector<int>{100, 101, 102, 103}; }, always_strong);
            check_adapter<A>(name + ": constructor from a compare and a container", n, [](auto& s) {
                    typename A::container_type copy(s.src.begin(), s.src.end());
                    Opened<A> made(typename A::value_compare(), copy);
                    s.a.swap(made);
                }, [](auto&) { return std::vector<int>{100, 101, 102, 103}; }, always_strong);
            check_adapter<A>(name + ": constructor from a range and a container", n, [](auto& s) {
                    typename A::container_type copy(s.src.begin(), s.src.begin() + 2);
                    Opened<A> made(s.src.begin() + 2, s.src.end(), typename A::value_compare(), copy);
                    s.a.swap(made);
                }, [](auto&) { return std::vector<int>{100, 101, 102, 103}; }, always_strong);
            check_adapter<A>(name + ": operator=(const&)", n, [](auto& s) { s.a = *s.other; }, [](auto&) { return iota7(); }, never_strong);
        }
    }
}

TEST(ExceptionSafety_Test, Adapters) {
    queue_or_stack<sgcl::queue<Element>>("queue");
    queue_or_stack<sgcl::queue<Element, sgcl::list<Element>>>("queue over a list");
    queue_or_stack<sgcl::stack<Element>>("stack");
    queue_or_stack<sgcl::stack<Element, sgcl::vector<Element>>>("stack over a vector");
    priority_queue_of<sgcl::priority_queue<Element>>("priority_queue");
    priority_queue_of<sgcl::priority_queue<Element, sgcl::deque<Element>>>("priority_queue over a deque");
    priority_queue_of<sgcl::priority_queue<NothrowElement>>("priority_queue, a move that cannot throw");
}

namespace {
    // A weak_map (weak_multimap) of n objects to their values 0..n-1 (the
    // objects kept by the state, so that no entry dies), the arguments (a
    // new object and an old one, a value -1) and the values before
    template<class W>
    struct WeakState {
        using T = typename W::mapped_type;

        int base = int(T::alive);
        W m;
        sgcl::vector<sgcl::tracked_ptr<int>> keys;
        sgcl::tracked_ptr<int> fresh = sgcl::make_tracked<int>(-1);
        std::optional<T> arg;
        Content before;
        int n;

        explicit WeakState(int n)
        : n(n) {
            for (int i = 0; i < n; ++i) {
                keys.push_back(sgcl::make_tracked<int>(i));
                m.emplace(keys.back(), i);
            }
            arg.emplace(-1);
            before = content();
        }

        // The entries as object's int to value
        Content content() const {
            Content out;
            for (auto e : m) {
                out.emplace(*e.key, es::iv(e.value));
            }
            return out;
        }

        void accounted() const {
            EXPECT_EQ(int(std::distance(m.begin(), m.end())), int(m.size()));
            EXPECT_EQ(int(T::alive), base + int(m.size()) + (arg ? 1 : 0));
        }

        void reset() {
            m.clear();
            arg.reset();
        }
    };

    template<class W, class Op, class Expected, class Strong>
    void check_weak(const std::string& what, int n, Op op, Expected expected, Strong strong) {
        es::each_throw(what + " of " + std::to_string(n), [=] { return sgcl::make_tracked<WeakState<W>>(n); }, op,
            [&](WeakState<W>& s, bool threw, unsigned kind) {
                s.accounted();
                auto now = s.content();
                if (threw) {
                    if (strong(s, kind)) {
                        EXPECT_EQ(pairs(now), pairs(s.before)) << "the page promises the map as it was";
                    }
                } else {
                    EXPECT_EQ(pairs(now), pairs(expected(s)));
                }
            });
    }

    Content with(Content c, int k, int v, bool replace) {
        if (replace) {
            c.erase(k);
        }
        if (replace || !c.count(k)) {
            c.emplace(k, v);
        }
        return c;
    }
}

// weak_map and weak_multimap: an insertion whose value's construction
// throws inserts nothing; insert_or_assign of an object there leaves the
// value as its assignment did
TEST(ExceptionSafety_Test, WeakMaps) {
    using W = sgcl::weak_map<int, Element>;
    for (int n : {0, 1, 5}) {
        for (bool there : {false, true}) {
            if (there && !n) {
                continue;
            }
            const std::string which = there ? " of an object there" : " of a new object";
            auto object = [there](auto& s) { return there ? s.keys[size_t(s.n / 2)] : s.fresh; };
            auto k_of = [there](const auto& s) { return there ? s.n / 2 : -1; };
            check_weak<W>("weak_map: emplace" + which, n, [object](auto& s) { s.m.emplace(object(s), -1); },
                [k_of](auto& s) { return with(s.before, k_of(s), -1, false); }, always_strong);
            check_weak<W>("weak_map: insert(const T&)" + which, n, [object](auto& s) { s.m.insert(object(s), std::as_const(*s.arg)); },
                [k_of](auto& s) { return with(s.before, k_of(s), -1, false); }, always_strong);
            check_weak<W>("weak_map: insert(T&&)" + which, n, [object](auto& s) { s.m.insert(object(s), std::move(*s.arg)); },
                [k_of](auto& s) { return with(s.before, k_of(s), -1, false); }, always_strong);
            check_weak<W>("weak_map: operator[]" + which, n, [object](auto& s) { s.m[object(s)]; },
                [k_of](auto& s) { return with(s.before, k_of(s), 0, false); }, always_strong);
            check_weak<W>("weak_map: insert_or_assign" + which, n, [object](auto& s) { s.m.insert_or_assign(object(s), std::as_const(*s.arg)); },
                [k_of](auto& s) { return with(s.before, k_of(s), -1, true); }, [there](const auto&, unsigned) { return !there; });
            check_weak<sgcl::weak_multimap<int, Element>>("weak_multimap: emplace" + which, n, [object](auto& s) { s.m.emplace(object(s), -1); },
                [k_of](auto& s) { Content c = s.before; c.emplace(k_of(s), -1); return c; }, always_strong);
        }
    }
}

namespace {
    struct Watched {
        int id;
    };

    // A function whose copy and move are the element's (they throw at
    // their countdowns), counting its calls
    struct OnExpire {
        Element held{0};
        int* calls = nullptr;

        void operator()(sgcl::tracked_ptr<Watched>) const {
            ++*calls;
        }
    };

    struct QueueState {
        int base = int(Element::alive);
        sgcl::expiry_queue<Watched> q;
        sgcl::tracked_ptr<Watched> object = sgcl::make_tracked<Watched>(1);
        std::optional<OnExpire> f;
        int calls = 0;
        size_t before = 0;

        QueueState() {
            f.emplace();
            f->calls = &calls;
            q.watch(object, *f);
            before = q.size();
        }

        void reset() {
            q.clear();
            f.reset();
        }
    };
}

// expiry_queue::watch: the function is made before the cell, so a copy or
// a move of it that throws leaves the queue as it was; a drain whose
// function throws has taken that entry out, and the next drain calls the
// entries it had not reached
TEST(ExceptionSafety_Test, ExpiryQueue) {
    for (bool moved : {false, true}) {
        es::each_throw(moved ? "expiry_queue: watch(object, F&&)" : "expiry_queue: watch(object, const F&)",
            [] { return sgcl::make_tracked<QueueState>(); },
            [moved](QueueState& s) {
                if (moved) {
                    s.q.watch(s.object, std::move(*s.f));
                } else {
                    s.q.watch(s.object, std::as_const(*s.f));
                }
            },
            [](QueueState& s, bool threw, unsigned) {
                EXPECT_EQ(s.q.size(), threw ? s.before : s.before + 1);
                EXPECT_EQ(int(Element::alive), s.base + int(s.q.size()) + (s.f ? 1 : 0));
                EXPECT_EQ(s.calls, 0);
            });
    }
    // a function that throws at its second call
    sgcl::expiry_queue<Watched> q;
    int calls = 0;
    off_frame([&] {
        for (int i = 0; i < 4; ++i) {
            q.watch(sgcl::make_tracked<Watched>(i), [&](sgcl::tracked_ptr<Watched>) {
                if (++calls == 2) {
                    throw std::runtime_error("on_expire");
                }
            });
        }
    });
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_THROW(q.drain(), std::runtime_error);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(q.size(), 2u);   // the entry that threw is gone, the two not reached stay
    EXPECT_EQ(q.drain(), 2u);
    EXPECT_EQ(calls, 4);
    EXPECT_TRUE(q.empty());
}
