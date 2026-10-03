//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of the immutable containers and their builders: every
// operation that makes an element run with a throw at each of its points
// in turn (the element's construction, copy and move: tests/throwing.h),
// and after each run checked against what its page says in Exceptions. A
// container is never changed, so the one the operation was called on
// holds what it held whatever threw, and no new version is made; a run
// without a throw gives the new version. A builder holds after a throw
// what it held before the call, and the container it was thawed from is
// untouched.
#include "tests/throwing.h"

#include <forward_list>
#include <map>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

using throwing::Val;

namespace {
    // A Val whose move cannot throw: a builder moves the elements of a
    // node it owns along in place only with such a move (else it copies
    // the node), so both roads are taken
    struct Rel {
        int v = 0;

        Rel(int x)
        : v(x) {
            throwing::tick(throwing::countdown.construct, "construct");
        }

        Rel(const Rel& o)
        : v(o.v) {
            throwing::tick(throwing::countdown.copy, "copy");
        }

        Rel(Rel&& o) noexcept
        : v(o.v) {
        }

        friend bool operator==(const Rel& a, const Rel& b) noexcept {
            return a.v == b.v;
        }
    };

    // The hashes of the keys, by the int: the int itself (1, 33 and 65
    // under one slot of the root, apart below it: a subtrie, a split) and
    // its residue mod 8 (1, 33 and 65 one chain)
    template<int Mod>
    struct IntHash {
        size_t operator()(int k) const noexcept {
            return Mod ? size_t(k % Mod) : size_t(k);
        }

        size_t operator()(const Val& k) const noexcept {
            return (*this)(k.v);
        }

        size_t operator()(const Rel& k) const noexcept {
            return (*this)(k.v);
        }
    };

    int iv(int x) {
        return x;
    }

    int iv(const Val& x) {
        return x.v;
    }

    int iv(const Rel& x) {
        return x.v;
    }

    std::vector<int> iota(int n, int from = 0) {
        std::vector<int> v(static_cast<size_t>(n));
        std::iota(v.begin(), v.end(), from);
        return v;
    }

    // The ints of a sequence, in its order
    template<class C>
    std::vector<int> ints(const C& c) {
        std::vector<int> out;
        for (auto& e : c) {
            out.push_back(iv(e));
        }
        EXPECT_EQ(out.size(), c.size()) << "size() against the elements walked";
        return out;
    }

    // What a map holds, key to value, or a set, key to 0: walked, the
    // count against size(), every key found by a lookup
    template<class C>
    std::map<int, int> content(const C& c) {
        std::map<int, int> out;
        size_t n = 0;
        for (auto& e : c) {
            ++n;
            if constexpr (requires { e.first; }) {
                EXPECT_TRUE(out.emplace(iv(e.first), iv(e.second)).second) << "key " << iv(e.first) << " walked twice";
                EXPECT_TRUE(c.contains(e.first)) << "key " << iv(e.first) << " walked but not found";
            } else {
                EXPECT_TRUE(out.emplace(iv(e), 0).second) << "key " << iv(e) << " walked twice";
                EXPECT_TRUE(c.contains(e)) << "key " << iv(e) << " walked but not found";
            }
        }
        EXPECT_EQ(n, c.size()) << "size() against the elements walked";
        return out;
    }

    // The keys of every map and set: 1, 33 and 65 one subtrie (identity)
    // or one chain (mod 8), as 3, 11 and 19 are under the mod; a value is
    // its key times ten
    const std::vector<int> Keys = {1, 33, 65, 2, 3, 11, 19, 4};

    std::map<int, int> keyed(const std::vector<int>& keys, bool set = false) {
        std::map<int, int> out;
        for (int k : keys) {
            out[k] = set ? 0 : k * 10;
        }
        return out;
    }

    // A change of `before`: `key` to `value` (an insert, a set), or out (erase)
    std::map<int, int> changed(std::map<int, int> before, int key, int value) {
        before[key] = value;
        return before;
    }

    std::map<int, int> without(std::map<int, int> before, int key) {
        before.erase(key);
        return before;
    }

    // The type of a map's values, of a set's keys
    template<class C>
    struct ValueOf {
        using type = typename C::key_type;
    };

    template<class C> requires requires { typename C::mapped_type; }
    struct ValueOf<C> {
        using type = typename C::mapped_type;
    };

    template<class C>
    using value_of = typename ValueOf<C>::type;
}

namespace {
    using V = sgcl::immutable::vector<Val>;

    // A vector of n elements 0..n-1 and the arguments of the operation,
    // made before the countdown is set
    struct VectorState {
        explicit VectorState(int n)
        : n(n) {
            for (int i : range(n)) {
                v = v.push_back(Val(i));
            }
            for (int i : range(70)) {
                source.emplace_back(Val(100 + i));
            }
        }

        int n;
        V v;
        V out;
        Val arg = Val(-1);
        std::vector<Val> source;
    };

    // The vector called on as it was; the new one what `expected` says
    // when nothing threw, none made when something did
    void check_vector(VectorState& s, bool threw, const std::vector<int>& expected) {
        EXPECT_EQ(ints(s.v), iota(s.n));
        if (threw) {
            EXPECT_TRUE(s.out.empty());
        } else {
            EXPECT_EQ(ints(s.out), expected);
        }
    }

    std::vector<int> plus(std::vector<int> v, int x) {
        v.push_back(x);
        return v;
    }

    std::vector<int> replaced(std::vector<int> v, size_t i, int x) {
        v[i] = x;
        return v;
    }
}

// push_back, emplace_back, pop_back and set across the shapes of the trie:
// empty, the tail alone, the tail full, one leaf in the trie, a full root
// and a second level begun; and the constructor from a range. The vector
// called on never changes (vector/push_back.md, pop_back.md, set.md)
TEST(ImExceptions_Tests, Vector) {
    for (int n : {0, 1, 31, 32, 33, 1056, 1057}) {
        const std::string at = " of " + std::to_string(n);
        auto setup = [n] {
            return make_tracked<VectorState>(n);
        };
        throwing::each_kind("push_back(const T&)" + at, setup, [](VectorState& s) {
            s.out = s.v.push_back(std::as_const(s.arg));
        }, [](VectorState& s, bool threw, bool) {
            check_vector(s, threw, plus(iota(s.n), -1));
            EXPECT_EQ(s.arg.v, -1);
        });
        throwing::each_kind("push_back(T&&)" + at, setup, [](VectorState& s) {
            s.out = s.v.push_back(std::move(s.arg));
        }, [](VectorState& s, bool threw, bool) {
            check_vector(s, threw, plus(iota(s.n), -1));
        });
        throwing::each_kind("emplace_back(int)" + at, setup, [](VectorState& s) {
            s.out = s.v.emplace_back(-1);
        }, [](VectorState& s, bool threw, bool) {
            check_vector(s, threw, plus(iota(s.n), -1));
        });
        if (n == 0) {
            continue;
        }
        throwing::each_kind("pop_back" + at, setup, [](VectorState& s) {
            s.out = s.v.pop_back();
        }, [](VectorState& s, bool threw, bool) {
            check_vector(s, threw, iota(s.n - 1));
        });
        for (size_t i : {size_t(0), size_t(n - 1)}) {
            throwing::each_kind("set(" + std::to_string(i) + ")" + at, setup, [i](VectorState& s) {
                s.out = s.v.set(i, std::as_const(s.arg));
            }, [i](VectorState& s, bool threw, bool) {
                check_vector(s, threw, replaced(iota(s.n), i, -1));
            });
            throwing::each_kind("set(" + std::to_string(i) + ", T&&)" + at, setup, [i](VectorState& s) {
                s.out = s.v.set(i, std::move(s.arg));
            }, [i](VectorState& s, bool threw, bool) {
                check_vector(s, threw, replaced(iota(s.n), i, -1));
            });
        }
    }
    throwing::each_kind("vector(first, last)", [] { return make_tracked<VectorState>(0); }, [](VectorState& s) {
        s.out = V(s.source.begin(), s.source.end());
    }, [](VectorState& s, bool threw, bool) {
        check_vector(s, threw, iota(70, 100));
        EXPECT_EQ(ints(s.source), iota(70, 100));
    });
}

namespace {
    using L = sgcl::immutable::list<Val>;

    // A list of n elements 0..n-1 and the arguments of the operation
    struct ListState {
        explicit ListState(int n)
        : n(n) {
            for (int i = n - 1; i >= 0; --i) {
                l = l.push_front(Val(i));
            }
            for (int i : range(5)) {
                source.emplace_back(Val(100 + i));
            }
            forward.assign(source.begin(), source.end());
        }

        int n;
        L l;
        L out;
        Val arg = Val(-1);
        std::vector<Val> source;
        std::forward_list<Val> forward;
    };

    void check_list(ListState& s, bool threw, const std::vector<int>& expected) {
        EXPECT_EQ(ints(s.l), iota(s.n));
        if (threw) {
            EXPECT_TRUE(s.out.empty());
        } else {
            EXPECT_EQ(ints(s.out), expected);
        }
    }

    std::vector<int> front_plus(std::vector<int> v, int x) {
        v.insert(v.begin(), x);
        return v;
    }
}

// push_front, emplace_front and reverse of an empty list, one of one
// element and a longer one; the constructors from a range walked back
// (bidirectional), through a buffer (forward only: the elements moved out
// of it, a throwing move among the points) and from a range object
TEST(ImExceptions_Tests, List) {
    for (int n : {0, 1, 5}) {
        const std::string at = " of " + std::to_string(n);
        auto setup = [n] {
            return make_tracked<ListState>(n);
        };
        throwing::each_kind("push_front(const T&)" + at, setup, [](ListState& s) {
            s.out = s.l.push_front(std::as_const(s.arg));
        }, [](ListState& s, bool threw, bool) {
            check_list(s, threw, front_plus(iota(s.n), -1));
            EXPECT_EQ(s.arg.v, -1);
        });
        throwing::each_kind("push_front(T&&)" + at, setup, [](ListState& s) {
            s.out = s.l.push_front(std::move(s.arg));
        }, [](ListState& s, bool threw, bool) {
            check_list(s, threw, front_plus(iota(s.n), -1));
        });
        throwing::each_kind("emplace_front(int)" + at, setup, [](ListState& s) {
            s.out = s.l.emplace_front(-1);
        }, [](ListState& s, bool threw, bool) {
            check_list(s, threw, front_plus(iota(s.n), -1));
        });
        throwing::each_kind("reverse" + at, setup, [](ListState& s) {
            s.out = s.l.reverse();
        }, [](ListState& s, bool threw, bool) {
            auto r = iota(s.n);
            std::reverse(r.begin(), r.end());
            check_list(s, threw, r);
        });
    }
    auto setup = [] {
        return make_tracked<ListState>(0);
    };
    auto check = [](ListState& s, bool threw, bool) {
        check_list(s, threw, iota(5, 100));
        EXPECT_EQ(ints(s.source), iota(5, 100));
    };
    throwing::each_kind("list(first, last), bidirectional", setup, [](ListState& s) {
        s.out = L(s.source.begin(), s.source.end());
    }, check);
    throwing::each_kind("list(first, last), forward", setup, [](ListState& s) {
        s.out = L(s.forward.begin(), s.forward.end());
    }, [](ListState& s, bool threw, bool fired) {
        check_list(s, threw, iota(5, 100));
        std::vector<int> forward;
        for (auto& x : s.forward) {
            forward.push_back(x.v);
        }
        EXPECT_EQ(forward, iota(5, 100));
    });
    throwing::each_kind("list(range)", setup, [](ListState& s) {
        s.out = L(s.source);
    }, check);
}

namespace {
    // A map (or set) of Keys and the arguments of the operation: `key`
    // and `value` of the element, its pair, and a range of Keys
    template<class C>
    struct AssocState {
        AssocState(int key, int value)
        : key(key)
        , value(value)
        , element(typename C::key_type(key), value_of<C>(value)) {
            for (int k : Keys) {
                if constexpr (requires { typename C::mapped_type; }) {
                    c = c.insert(typename C::key_type(k), typename C::mapped_type(k * 10));
                    range.emplace_back(typename C::key_type(k), typename C::mapped_type(k * 10));
                } else {
                    c = c.insert(typename C::key_type(k));
                    keys.emplace_back(k);
                }
            }
        }

        C c;
        C out;
        typename C::key_type key;
        value_of<C> value;
        std::pair<const typename C::key_type, value_of<C>> element;
        std::vector<std::pair<const typename C::key_type, value_of<C>>> range;
        std::vector<typename C::key_type> keys;
    };

    // The container called on as it was; the new one `expected` when
    // nothing threw, none made when something did
    template<class S>
    void check_assoc(S& s, bool threw, const std::map<int, int>& expected, bool set = false) {
        EXPECT_EQ(content(s.c), keyed(Keys, set));
        if (threw) {
            EXPECT_TRUE(s.out.empty());
        } else {
            EXPECT_EQ(content(s.out), expected);
        }
    }

    // Every change of a map of Vals: an insert of a new key (by the
    // pair, by copies, by moves, emplaced), a set of a key there and of
    // a new one, an erase, and the build from a range
    template<class M>
    void map_changes(const std::string& name, int fresh, int present) {
        using State = AssocState<M>;
        auto setup = [fresh](int key) {
            return [key, fresh] {
                return make_tracked<State>(key, key == fresh ? -1 : -2);
            };
        };
        const auto before = keyed(Keys);
        const auto inserted = changed(before, fresh, -1);
        const std::string at = name + " key " + std::to_string(fresh);
        throwing::each_kind("insert(const Key&, const T&) " + at, setup(fresh), [](State& s) {
            s.out = s.c.insert(std::as_const(s.key), std::as_const(s.value));
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, inserted);
            EXPECT_EQ(iv(s.key), fresh);
        });
        throwing::each_kind("insert(Key&&, T&&) " + at, setup(fresh), [](State& s) {
            s.out = s.c.insert(std::move(s.key), std::move(s.value));
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, inserted);
        });
        throwing::each_kind("insert(const value_type&) " + at, setup(fresh), [](State& s) {
            s.out = s.c.insert(s.element);
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, inserted);
        });
        throwing::each_kind("emplace(const Key&, int) " + at, setup(fresh), [](State& s) {
            s.out = s.c.emplace(s.key, -1);
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, inserted);
        });
        throwing::each_kind("set(const Key&, const T&) " + at, setup(fresh), [](State& s) {
            s.out = s.c.set(std::as_const(s.key), std::as_const(s.value));
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, inserted);
        });
        const std::string there = name + " key " + std::to_string(present);
        throwing::each_kind("set(Key&&, T&&) " + there, setup(present), [](State& s) {
            s.out = s.c.set(std::move(s.key), std::move(s.value));
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, changed(before, present, -2));
        });
        throwing::each_kind("insert of a key there " + there, setup(present), [](State& s) {
            s.out = s.c.insert(std::as_const(s.key), std::as_const(s.value));
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, before);   // the same map: the key kept with its value
        });
        throwing::each_kind("erase " + there, setup(present), [](State& s) {
            s.out = s.c.erase(s.key);
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, without(before, present));
        });
        throwing::each_kind("map(first, last) " + name, setup(fresh), [](State& s) {
            s.out = M(s.range.begin(), s.range.end());
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, before);
        });
    }

    template<class S>
    void set_changes(const std::string& name, int fresh, int present) {
        using State = AssocState<S>;
        auto setup = [](int key) {
            return [key] {
                return make_tracked<State>(key, 0);
            };
        };
        const auto before = keyed(Keys, true);
        const std::string at = name + " key " + std::to_string(fresh);
        throwing::each_kind("insert(const Key&) " + at, setup(fresh), [](State& s) {
            s.out = s.c.insert(std::as_const(s.key));
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, changed(before, fresh, 0), true);
            EXPECT_EQ(iv(s.key), fresh);
        });
        throwing::each_kind("insert(Key&&) " + at, setup(fresh), [](State& s) {
            s.out = s.c.insert(std::move(s.key));
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, changed(before, fresh, 0), true);
        });
        const std::string there = name + " key " + std::to_string(present);
        throwing::each_kind("erase " + there, setup(present), [](State& s) {
            s.out = s.c.erase(s.key);
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, without(before, present), true);
        });
        throwing::each_kind("set(first, last) " + name, setup(fresh), [](State& s) {
            s.out = S(s.keys.begin(), s.keys.end());
        }, [&](State& s, bool threw, bool) {
            check_assoc(s, threw, before, true);
        });
    }
}

// Every change of a map and of a set: a new key in a free slot of the
// root (5), one under a subtrie or at the end of a chain (97), one that
// splits an element's slot or joins a chain (35); a key there inside the
// subtrie or the chain (33), one alone in its slot (2) and the head of a
// chain (1). The map or set called on never changes (map/insert.md,
// set.md, emplace.md, erase.md; set/insert.md, erase.md)
TEST(ImExceptions_Tests, MapAndSet) {
    using M0 = sgcl::immutable::map<Val, Val, IntHash<0>>;
    using M8 = sgcl::immutable::map<Val, Val, IntHash<8>>;
    using S0 = sgcl::immutable::set<Val, IntHash<0>>;
    using S8 = sgcl::immutable::set<Val, IntHash<8>>;
    for (auto [fresh, present] : {std::pair{5, 33}, std::pair{97, 2}, std::pair{35, 1}}) {
        map_changes<M0>("map, identity,", fresh, present);
        map_changes<M8>("map, mod 8,", fresh, present);
        set_changes<S0>("set, identity,", fresh, present);
        set_changes<S8>("set, mod 8,", fresh, present);
    }
}

namespace {
    // A builder over Keys and the arguments of the operation. `shared`:
    // thawed from `frozen`, whose nodes it copies the first time a change
    // goes through them; else filled by inserts, its nodes its own
    template<class C>
    struct BuilderState {
        BuilderState(int key, int value, bool shared)
        : key(key)
        , value(value) {
            for (int k : Keys) {
                insert(k);
            }
            if (shared) {
                frozen = b.freeze();
                b = frozen.thaw();
            }
        }

        void insert(int k) {
            if constexpr (requires { typename C::mapped_type; }) {
                b.insert(typename C::key_type(k), typename C::mapped_type(k * 10));
            } else {
                b.insert(typename C::key_type(k));
            }
        }

        typename C::builder b;
        C frozen;
        typename C::key_type key;
        value_of<C> value;
    };

    // The builder: the elements it held when something threw, `expected`
    // when nothing did, its trie whole either way (frozen and walked);
    // what it was thawed from untouched
    template<class S>
    void check_builder(S& s, bool threw, const std::map<int, int>& expected, bool set = false) {
        auto now = s.b.freeze();
        EXPECT_EQ(content(now), threw ? keyed(Keys, set) : expected);
        EXPECT_EQ(s.b.size(), now.size());
        if (!s.frozen.empty()) {
            EXPECT_EQ(content(s.frozen), keyed(Keys, set));
        }
    }

    template<class M>
    void map_builder_changes(const std::string& name, int fresh, int present) {
        for (bool shared : {false, true}) {
            using State = BuilderState<M>;
            auto setup = [shared](int key, int value) {
                return [=] {
                    return make_tracked<State>(key, value, shared);
                };
            };
            const auto before = keyed(Keys);
            const auto inserted = changed(before, fresh, -1);
            const std::string at = name + (shared ? " shared," : " own,") + " key " + std::to_string(fresh);
            throwing::each_kind("builder insert(const Key&, const T&) " + at, setup(fresh, -1), [](State& s) {
                EXPECT_TRUE(s.b.insert(std::as_const(s.key), std::as_const(s.value)));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, inserted);
            });
            throwing::each_kind("builder insert(Key&&, T&&) " + at, setup(fresh, -1), [](State& s) {
                EXPECT_TRUE(s.b.insert(std::move(s.key), std::move(s.value)));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, inserted);
            });
            throwing::each_kind("builder emplace(const Key&, int) " + at, setup(fresh, -1), [](State& s) {
                EXPECT_TRUE(s.b.emplace(s.key, -1));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, inserted);
            });
            throwing::each_kind("builder set(const Key&, const T&) " + at, setup(fresh, -1), [](State& s) {
                EXPECT_TRUE(s.b.set(std::as_const(s.key), std::as_const(s.value)));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, inserted);
            });
            const std::string there = name + (shared ? " shared," : " own,") + " key " + std::to_string(present);
            throwing::each_kind("builder set(Key&&, T&&) " + there, setup(present, -2), [](State& s) {
                EXPECT_FALSE(s.b.set(std::move(s.key), std::move(s.value)));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, changed(before, present, -2));
            });
            throwing::each_kind("builder erase " + there, setup(present, 0), [](State& s) {
                EXPECT_TRUE(s.b.erase(s.key));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, without(before, present));
            });
        }
    }

    template<class S>
    void set_builder_changes(const std::string& name, int fresh, int present) {
        for (bool shared : {false, true}) {
            using State = BuilderState<S>;
            auto setup = [shared](int key) {
                return [=] {
                    return make_tracked<State>(key, 0, shared);
                };
            };
            const auto before = keyed(Keys, true);
            const std::string at = name + (shared ? " shared," : " own,") + " key " + std::to_string(fresh);
            throwing::each_kind("builder insert(const Key&) " + at, setup(fresh), [](State& s) {
                EXPECT_TRUE(s.b.insert(std::as_const(s.key)));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, changed(before, fresh, 0), true);
            });
            throwing::each_kind("builder insert(Key&&) " + at, setup(fresh), [](State& s) {
                EXPECT_TRUE(s.b.insert(std::move(s.key)));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, changed(before, fresh, 0), true);
            });
            const std::string there = name + (shared ? " shared," : " own,") + " key " + std::to_string(present);
            throwing::each_kind("builder erase " + there, setup(present), [](State& s) {
                EXPECT_TRUE(s.b.erase(s.key));
            }, [&](State& s, bool threw, bool) {
                check_builder(s, threw, without(before, present), true);
            });
        }
    }
}

// The builders' inserts, sets and erases over the shapes of MapAndSet,
// of a builder that owns its nodes and of one that shares them with the
// map it was thawed from; the elements of a Val (a move that may throw:
// the builder copies the node it changes) and of an int and a Rel (a
// move that cannot: the builder moves the elements of its own node
// along). After a throw the builder holds what it held, and the map is
// untouched (map-builder/insert.md, set.md, emplace.md, erase.md;
// set-builder/insert.md, erase.md)
TEST(ImExceptions_Tests, Builders) {
    using M0 = sgcl::immutable::map<Val, Val, IntHash<0>>;
    using M8 = sgcl::immutable::map<Val, Val, IntHash<8>>;
    using R0 = sgcl::immutable::map<int, Rel, IntHash<0>>;
    using R8 = sgcl::immutable::map<int, Rel, IntHash<8>>;
    using S0 = sgcl::immutable::set<Val, IntHash<0>>;
    using S8 = sgcl::immutable::set<Val, IntHash<8>>;
    using T0 = sgcl::immutable::set<Rel, IntHash<0>>;
    using T8 = sgcl::immutable::set<Rel, IntHash<8>>;
    for (auto [fresh, present] : {std::pair{5, 33}, std::pair{97, 2}, std::pair{35, 1}}) {
        map_builder_changes<M0>("map, identity,", fresh, present);
        map_builder_changes<M8>("map, mod 8,", fresh, present);
        map_builder_changes<R0>("map of Rel, identity,", fresh, present);
        map_builder_changes<R8>("map of Rel, mod 8,", fresh, present);
        set_builder_changes<S0>("set, identity,", fresh, present);
        set_builder_changes<S8>("set, mod 8,", fresh, present);
        set_builder_changes<T0>("set of Rel, identity,", fresh, present);
        set_builder_changes<T8>("set of Rel, mod 8,", fresh, present);
    }
}
