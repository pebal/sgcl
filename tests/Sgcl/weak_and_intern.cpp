//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The Sgcl interface over the concurrent weak containers and the pool:
// ConcurrentWeakDictionary, ConcurrentWeakHashSet, Intern, InternString.
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    struct Node {
        explicit Node(int v = 0) : value(v) { ++alive; }
        Node(const Node& o) : value(o.value) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Point {
        int x, y;
        bool operator==(const Point&) const = default;
    };

    struct PointHash {
        size_t operator()(const Point& p) const noexcept {
            return size_t(p.x) * 1000003u + size_t(p.y);
        }
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Sgcl_WeakAndIntern_Tests, ConcurrentWeakDictionaryAndHashSet) {
    using namespace Sgcl;
    settle();
    const int before = Node::alive.load();
    ConcurrentWeakDictionary<Node, int> wd;
    ConcurrentWeakHashSet<Node> ws;
    Ptr<Node> kept;
    off_frame([&] {                      // a key made and dropped in a frame below
        kept = Make<Node>(1);
        Ptr k = Make<Node>(2);
        EXPECT_TRUE(wd.Add(k, 1));
        EXPECT_FALSE(wd.Add(k, 2));
        EXPECT_TRUE(wd.Emplace(kept, 5));
        EXPECT_EQ(*wd.TryGet(k), 1);
        EXPECT_EQ(wd.Find(kept)->value, 5);
        EXPECT_EQ(wd.GetOrAdd(kept, 9)->value, 5);
        EXPECT_TRUE(wd.ContainsKey(k));
        EXPECT_EQ(wd.Count(), 2u);
        int sum = 0;
        for (auto [key, value] : wd) {
            sum += value;
        }
        EXPECT_EQ(sum, 6);
        EXPECT_TRUE(ws.Add(k));
        EXPECT_FALSE(ws.Add(k));
        EXPECT_TRUE(ws.Add(kept));
        EXPECT_TRUE(ws.Contains(k));
        EXPECT_EQ(*ws.Find(k), k.Inner());
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    EXPECT_EQ(wd.Count(), 2u);           // the dead entry not yet swept
    int seen = 0;
    for (auto [key, value] : wd) {       // and passed over
        ++seen;
        EXPECT_EQ(key, kept.Inner());
        EXPECT_EQ(value, 5);
    }
    EXPECT_EQ(seen, 1);
    EXPECT_FALSE(wd.TryGet(Ptr<Node>()));
    EXPECT_EQ(wd.Sweep(), 1u);
    EXPECT_EQ(ws.Sweep(), 1u);
    EXPECT_EQ(wd.Count(), 1u);
    EXPECT_EQ(ws.Count(), 1u);
    for (auto object : ws) {
        EXPECT_EQ(object->value, 1);
    }
    EXPECT_TRUE(wd.Remove(kept));
    EXPECT_FALSE(wd.Remove(kept));
    EXPECT_TRUE(wd.Find(kept) == wd.End());
    EXPECT_TRUE(ws.Remove(kept));
    EXPECT_TRUE(wd.IsEmpty() && ws.IsEmpty());
    wd.Add(kept, 1);
    ws.Add(kept);
    EXPECT_TRUE(wd.Remove(wd.Find(kept)) == wd.End());
    EXPECT_TRUE(ws.Remove(ws.Find(kept)) == ws.End());
    wd.Add(kept, 1);
    wd.Clear();
    EXPECT_TRUE(wd.IsEmpty());
}

TEST(Sgcl_WeakAndIntern_Tests, ConcurrentWeakDictionaryManyThreads) {
    using namespace Sgcl;
    settle();
    const int before = Node::alive.load();
    const int threads = 4;
    const int per_thread = 2000;
    ConcurrentWeakDictionary<Node, int> wd;
    List<Ptr<Node>> kept;
    off_frame([&] {
        ConcurrentQueue<Ptr<Node>> kept_by_threads;   // what the threads keep, handed over
        auto round = [&] {                 // two rounds with a collection between: the second sweeps the first's dead entries out
            std::vector<std::thread> ts;
            for (int t = 0; t < threads; ++t) {
                ts.emplace_back([&, t] {
                    for (int i = 0; i < per_thread; ++i) {
                        Ptr node = Make<Node>(i);
                        if (!wd.Add(node, i)) {
                            ADD_FAILURE() << "added twice";
                        }
                        if (i % 400 == 0) {
                            kept_by_threads.Enqueue(node);
                        }
                        if (i % 500 == 499 && t == 0) {
                            collector::force_collect();
                        }
                    }
                });
            }
            for (auto& th : ts) {
                th.join();
            }
        };
        round();
        collector::force_collect(true);
        collector::force_collect(true);
        const size_t first = wd.Count();
        round();
        EXPECT_LT(wd.Count(), first + size_t(threads * per_thread));   // a sweep ran under the second round
        while (auto p = kept_by_threads.TryDequeue()) {
            kept.Add(*p);
        }
    });
    settle();
    wd.Sweep();
    EXPECT_EQ(wd.Count(), kept.Count());
    EXPECT_EQ(Node::alive.load(), before + (int)kept.Count());
    for (auto& p : kept) {
        EXPECT_EQ(*wd.TryGet(p), p->value);
    }
}

TEST(Sgcl_WeakAndIntern_Tests, InternAndInternString) {
    using namespace Sgcl;
    using Points = Intern<Point, PointHash>;
    Ptr<const Point> a = Points::Make({1, 2});
    Ptr<const Point> b = Points::Make({1, 2});
    Ptr<const Point> c = Points::Make({2, 1});
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_EQ(a->y, 2);
    EXPECT_EQ(Points::Pool().Find(Point{1, 2}), a);
    EXPECT_EQ(Points::Pool().Find(Point{3, 3}), nullptr);
    EXPECT_EQ(Points::Pool().Count(), 2u);
    Intern<Point, PointHash> local;
    Ptr<const Point> d = local.Get({1, 2});
    EXPECT_NE(d, a);                     // a pool of its own
    EXPECT_EQ(local.Get(Point{1, 2}), d);
    EXPECT_EQ(local.Count(), 1u);
    EXPECT_FALSE(local.IsEmpty());
    local.Clear();
    EXPECT_TRUE(local.IsEmpty());

    String x = InternString("abc");
    String y = Intern<String>::Make(std::string_view("abc"));
    String z = Intern<String>::Make(String("abc"));
    String w = Intern<String>::Make("abc");
    EXPECT_EQ(x.Object(), y.Object());
    EXPECT_EQ(x.Object(), z.Object());
    EXPECT_EQ(x.Object(), w.Object());
    EXPECT_EQ(x, "abc");
    EXPECT_EQ(Intern<String>::Pool().Find("abc").Object(), x.Object());
    EXPECT_EQ(Intern<String>::Pool().Find("none").Object(), nullptr);
    String own("own");
    EXPECT_EQ(Intern<String>::Make(own).Object(), own.Object());   // the string itself enters
    EXPECT_EQ(InternString("").Object(), nullptr);
    Intern<String> names;
    EXPECT_EQ(names.Get("n").Object(), names.Get(String("n")).Object());
    EXPECT_NE(names.Get("n").Object(), x.Object());
    EXPECT_EQ(names.Count(), 1u);
    names.Sweep();
    EXPECT_EQ(names.Count(), 1u);        // alive: kept by nothing but this frame, not swept while it is
}

TEST(Sgcl_WeakAndIntern_Tests, ADeadInternedObjectIsMadeAgain) {
    using namespace Sgcl;
    settle();
    const int before = Node::alive.load();
    struct NodeHash {
        size_t operator()(const Node& n) const noexcept { return std::hash<int>()(n.value); }
    };
    struct NodeEqual {
        bool operator()(const Node& a, const Node& b) const noexcept { return a.value == b.value; }
    };
    Intern<Node, NodeHash, NodeEqual> pool;
    off_frame([&] {
        Ptr<const Node> p = pool.Get(Node(3));
        EXPECT_EQ(pool.Get(Node(3)), p);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
    EXPECT_EQ(pool.Find(Node(3)), nullptr);
    Ptr<const Node> again = pool.Get(Node(3));
    EXPECT_EQ(again->value, 3);
    EXPECT_EQ(pool.Count(), 2u);
    EXPECT_EQ(pool.Sweep(), 1u);
    EXPECT_EQ(pool.Count(), 1u);
}

TEST(Sgcl_WeakAndIntern_Tests, InternManyThreads) {
    using namespace Sgcl;
    const int threads = 4;
    const int rounds = 1000;
    off_frame([&] {
        Intern<Point, PointHash> pool;
        List<List<Ptr<const Point>>> got(threads, List<Ptr<const Point>>(5));   // sized before the threads start
        List<List<String>> names(threads, List<String>(5));
        std::vector<std::thread> ts;
        for (int t = 0; t < threads; ++t) {
            ts.emplace_back([&, t] {
                for (int r = 0; r < rounds; ++r) {
                    for (int v : Range(5)) {
                        Ptr<const Point> p = pool.Get({v, v});
                        String s = InternString("k" + std::to_string(v));
                        if (r == 0) {
                            got[t][v] = p;
                            names[t][v] = s;
                        } else if (got[t][v] != p || names[t][v].Object() != s.Object()) {
                            ADD_FAILURE() << "two objects for one value";
                        }
                    }
                }
            });
        }
        for (auto& th : ts) {
            th.join();
        }
        for (int v : Range(5)) {
            for (int t = 1; t < threads; ++t) {
                EXPECT_EQ(got[t][v], got[0][v]);
                EXPECT_EQ(names[t][v].Object(), names[0][v].Object());
            }
        }
        EXPECT_EQ(pool.Count(), 5u);
    });
}
