//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The Sgcl interface: every class over its sgcl type, the methods
// forward, the pointers inside are traced, the frames are managed.
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <atomic>
#include <chrono>
#include <coroutine>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Node {
        explicit Node(int v = 0) : value(v) { ++alive; }
        Node(const Node& o) : value(o.value) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        Sgcl::Ptr<Node> next;
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Base {
        virtual ~Base() = default;
    };

    struct Derived : Base {
        int value = 7;
    };

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Sgcl_Tests, PtrMakeAndTheObjectLivesWhileHeld) {
    using namespace Sgcl;
    settle();
    const int before = Node::alive.load();
    Ptr<Node> n;
    Ptr<Node> copy;
    off_frame([&] {                      // the objects made in a frame below: its spills are cleared by settle
        n = Make<Node>(1);
        n->next = Make<Node>(2);
        EXPECT_EQ(n->next->value, 2);
        EXPECT_EQ(Node::alive.load(), before + 2);
        n->next = nullptr;
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    off_frame([&] {
        copy = n;
        EXPECT_EQ(copy, n);
        EXPECT_NE(copy, nullptr);
        n = nullptr;
        EXPECT_TRUE(copy);
        EXPECT_EQ(copy->value, 1);
        copy.Reset();
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Sgcl_Tests, PtrTypesAndCasts) {
    using namespace Sgcl;
    Ptr<Base> b = Make<Derived>();
    EXPECT_TRUE(b.Is<Derived>());
    EXPECT_EQ(b.As<Derived>()->value, 7);
    EXPECT_EQ(b.Type(), typeid(Derived));
    Ptr<Derived> d = DynamicCast<Derived>(b);
    EXPECT_EQ(d, b);
    EXPECT_EQ(StaticCast<Base>(d), b);
    EXPECT_EQ(std::hash<Ptr<Base>>()(b), std::hash<Base*>()(b.Get()));
}

TEST(Sgcl_Tests, UniqueRootAndWeakPtrs) {
    using namespace Sgcl;
    settle();
    const int before = Node::alive.load();
    std::vector<RootPtr<Node>> roots;   // a std container: RootPtr lives there
    WeakPtr<Node> w;
    off_frame([&] {
        UniquePtr u = Make<Node>(3);
        EXPECT_TRUE(u.Is<Node>());
        Ptr<Node> p = std::move(u);
        EXPECT_FALSE(u);
        roots.emplace_back(p);
        EXPECT_EQ(roots[0], p);
        EXPECT_EQ(roots[0].GetPtr(), p);
        w = p;
        EXPECT_EQ(w.Lock(), p);
        EXPECT_FALSE(w.IsExpired());
    });
    settle();
    EXPECT_FALSE(w.IsExpired());         // the RootPtr holds it
    off_frame([&] {
        roots.clear();
    });
    settle();
    EXPECT_TRUE(w.IsExpired());
    EXPECT_EQ(w.Lock(), nullptr);
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Sgcl_Tests, AtomicPtrAndAtomicRef) {
    using namespace Sgcl;
    Atomic<Ptr<Node>> head;
    Ptr node = Make<Node>(1);
    head.Store(node);
    Ptr<Node> expected = node;
    EXPECT_TRUE(head.CompareExchange(expected, nullptr));
    EXPECT_EQ(head.Load(), nullptr);
    EXPECT_EQ(head.Exchange(node), nullptr);
    EXPECT_EQ(head.Load(), node);
    AtomicRef next(node->next);
    next.Store(Make<Node>(2));
    EXPECT_EQ(next.Load()->value, 2);
    Atomic<int> counter = 1;
    counter += 2;
    EXPECT_EQ(counter.FetchAdd(1), 3);
    EXPECT_EQ(counter.Load(), 4);
    Atomic<String> name = String("a");
    String seen = name.Load();
    EXPECT_TRUE(name.CompareExchange(seen, String("b")));
    EXPECT_EQ(name.Load(), "b");
}

TEST(Sgcl_Tests, StringIsImmutableAndShared) {
    using namespace Sgcl;
    String s = "hello";
    String t = s + " world";
    EXPECT_EQ(t.Length(), 11u);
    EXPECT_TRUE(t.StartsWith("hello") && t.EndsWith("world") && t.Contains(' '));
    EXPECT_EQ(t.IndexOf("world"), 6u);
    EXPECT_EQ(t.LastIndexOf('o'), 7u);
    EXPECT_EQ(t.IndexOf('z'), String::NoPosition);
    EXPECT_EQ(t.Substring(6), "world");
    EXPECT_EQ(t.Substring(0, 5), s);
    EXPECT_TRUE(t.Equals(String("hello world")));
    EXPECT_LT(s, t);
    EXPECT_EQ(std::hash<String>()(t), String("hello world").Hash());
    EXPECT_EQ(t.ToStd(), std::string("hello world"));
    EXPECT_EQ(ToString(42) + ToString(true), "42true");
    EXPECT_EQ(*Parse<int>("42"), 42);                      // the reverse: None unless the text is exactly one number
    EXPECT_EQ(*Parse<int>(String("-7")), -7);
    EXPECT_EQ(Parse<int>("4x"), None);
    EXPECT_EQ(Parse<unsigned>("-1"), None);
    EXPECT_EQ(*Parse<int>("ff", 16), 255);
    EXPECT_EQ(*Parse<double>("2.5"), 2.5);
    EXPECT_EQ(Parse<float>(""), None);
    EXPECT_TRUE(*Parse<bool>("true") && !*Parse<bool>("false") && Parse<bool>("yes") == None);
    StringView view_of = t.View(6);                        // a view that holds the object
    EXPECT_EQ(view_of, "world");
    EXPECT_EQ(view_of.Object(), t.Object());
    EXPECT_EQ(view_of.Substring(1, 3), "orl");
    EXPECT_EQ(t.View().Trim().TrimPrefix("hello").TrimStart(), "world");
    EXPECT_EQ(view_of.ToString(), "world");
    EXPECT_EQ(String(t.View()).Object(), t.Object());       // the whole: the same object
    EXPECT_TRUE(view_of == String("world") && view_of != t && view_of.StartsWith('w') && view_of.IndexOf("ld") == 3);
    EXPECT_EQ(std::hash<StringView>()(view_of), String("world").Hash());
    List<StringView> views(t.Split(' '));                  // the pieces as views, each holding the string
    EXPECT_EQ(views.Count(), 2u);
    EXPECT_EQ(views[1].Object(), t.Object());
    List<String> parts(String("a,b,,c").Split(','));       // Split is a range of views; a List of Strings when kept
    ASSERT_EQ(parts.Count(), 4u);
    EXPECT_TRUE(parts[0] == "a" && parts[2].IsEmpty() && parts[3] == "c");
    EXPECT_EQ(String::Join(parts, "-"), "a-b--c");
    EXPECT_EQ(String::Join(String("a,b").Split(','), "+"), "a+b");   // or joined as it is
    EXPECT_EQ(std::ranges::distance(String("a::b").Split("::", 1)), 1);
    int words = 0;
    for (StringView word : String(" the quick\tfox ").SplitWords()) {
        words += !word.IsEmpty();
    }
    EXPECT_EQ(words, 3);
    EXPECT_EQ(String("  hi  ").Trim(), "hi");
    EXPECT_EQ(String("  hi  ").TrimStart(), "hi  ");
    EXPECT_EQ(String("  hi  ").TrimEnd(), "  hi");
    EXPECT_EQ(String("xhix").Trim("x"), "hi");
    EXPECT_EQ(t.TrimPrefix("hello "), "world");
    EXPECT_EQ(t.TrimSuffix(" world"), "hello");
    EXPECT_EQ(t.Replace("o", "0"), "hell0 w0rld");
    EXPECT_EQ(t.Replace('o', '0', 1), "hell0 world");
    EXPECT_EQ(t.Replace("zz", "y").Object(), t.Object());       // nothing replaced: the same object
    EXPECT_EQ(String("ab").Repeat(2), "abab");
    EXPECT_EQ(t.ToUpper(), "HELLO WORLD");
    EXPECT_EQ(t.ToLower().Object(), t.Object());                 // already lower: the same object
    Dictionary<String, int> ages = {{"alice", 30}};
    SortedDictionary<String, int> sorted = {{"alice", 30}};
    HashSet<String> hashed = {"alice"};
    SortedSet<String> names = {"alice"};
    std::string_view view = "alice";                              // a view converts to no String: the lookups are transparent
    EXPECT_EQ(*ages.Find(view), 30);
    EXPECT_EQ(*sorted.Find(view), 30);
    EXPECT_TRUE(ages.ContainsKey(view) && sorted.ContainsKey(view) && hashed.Contains(view) && names.Contains(view));
    EXPECT_EQ(std::hash<String>()(view), String("alice").Hash());
    int n = 0;
    for (char c : t) {
        n += c != ' ';
    }
    EXPECT_EQ(n, 10);
    String copy = t;
    EXPECT_EQ(copy.Data(), t.Data());   // the same object
}

TEST(Sgcl_Tests, ListIsAVectorWithTheNamesOfAList) {
    using namespace Sgcl;
    List<int> l = {3, 1, 2};
    l.Add(4);
    l.Insert(0, 0);
    l.AddRange({5, 6});
    EXPECT_EQ(l.Count(), 7u);
    EXPECT_EQ(l.IndexOf(4), 4u);
    EXPECT_EQ(l.IndexOf(9), NoIndex);
    EXPECT_TRUE(l.Contains(6));
    EXPECT_TRUE(l.Remove(1));
    EXPECT_FALSE(l.Remove(1));
    l.Sort();
    EXPECT_EQ(l.First(), 0);
    EXPECT_EQ(l.Last(), 6);
    EXPECT_EQ(l.RemoveAll([](int v) { return v % 2; }), 2u);
    EXPECT_EQ(*l.Find([](int v) { return v > 2; }), 4);
    l.Reverse();
    EXPECT_EQ(l[0], 6);
    int sum = 0;
    for (int v : l) {
        sum += v;
    }
    EXPECT_EQ(sum, 12);
    List<int> copy = l;
    copy.Add(1);
    EXPECT_NE(copy, l);              // a value: the copy is its own
    EXPECT_EQ(copy.Count(), l.Count() + 1);
    Ptr shared = Make<List<int>>(l);   // shared through a Ptr
    shared->Add(1);
    EXPECT_EQ(*shared, copy);
    l.Clear();
    EXPECT_TRUE(l.IsEmpty());
}

// The list and everything that touches it live in a frame of their own:
// an address of the list or of its buffer left in a register or a spill
// slot of the test's frame is a root of the conservative scan for as long
// as the frame lives (it kept the last fifty nodes alive at -O1 and at
// -O2 with the register allocation of one build), and a frame that has
// returned leaves neither: its registers are the caller's again, its
// slots are cleared by settle()
TEST(Sgcl_Tests, AListOfPtrsIsTraced) {
    using namespace Sgcl;
    settle();
    const int before = Node::alive.load();
    off_frame([&] {
        Ptr holder = Make<List<Ptr<Node>>>();
        off_frame([&] {
            for (int i = 0; i < 100; ++i) {
                holder->Add(Make<Node>(i));
            }
        });
        settle();
        EXPECT_EQ(Node::alive.load(), before + 100);
        off_frame([&] { holder->RemoveRange(0, 50); });
        settle();
        EXPECT_EQ(Node::alive.load(), before + 50);
    });   // holder dies with the frame
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

// Array<T, N> is constexpr throughout, the algorithms included, as
// sgcl::array<T, N> is
namespace {
    constexpr int sorted_last() {
        Sgcl::Array<int, 4> a = {3, 1, 4, 2};
        a.Sort();
        return a.Last();
    }
}

// A coroutine type of your own on ManagedFrame and FramePtr: the promise
// derives from ManagedFrame, get_return_object hands the handle to a
// FramePtr, and the frame's locals, parameters and promise members are
// roots while the FramePtr holds it
namespace {
    class Walker {
    public:
        struct promise_type : Sgcl::ManagedFrame {
            Sgcl::Ptr<Node> current;
            Walker get_return_object() { return Walker(std::coroutine_handle<promise_type>::from_promise(*this)); }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            std::suspend_always yield_value(Sgcl::Ptr<Node> n) noexcept { current = n; return {}; }
            void return_void() noexcept {}
            void unhandled_exception() { throw; }
        };
        bool Step() { _frame.Resume(); return !_frame.IsDone(); }
        const Sgcl::Ptr<Node>& Current() const { return _frame.Promise().current; }
        explicit operator bool() const noexcept { return static_cast<bool>(_frame); }
    private:
        explicit Walker(std::coroutine_handle<promise_type> h) : _frame(h) {}
        Sgcl::FramePtr<promise_type> _frame;
    };

    Walker Walk(Sgcl::Ptr<Node> head) {
        for (auto n = head; n; n = n->next) {
            co_yield n;
        }
    }
}

TEST(Sgcl_Tests, ACoroutineOfYourOwnOnAManagedFrame) {
    using namespace Sgcl;
    settle();
    const int before = Node::alive.load();
    off_frame([&] {
        Ptr<Node> head;
        for (int i = 0; i < 4; ++i) {
            Ptr n = Make<Node>(i);
            n->next = head;
            head = n;
        }
        Walker w = Walk(head);
        head = nullptr;                                    // the frame's parameter is the only root now
        int sum = 0;
        while (w.Step()) {
            sum += w.Current()->value;
            settle();                                      // the chain survives every cycle
        }
        EXPECT_EQ(sum, 6);
        EXPECT_TRUE(w);
        EXPECT_EQ(Node::alive.load(), before + 4);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);                 // the walker gone, the chain with it
}

TEST(Sgcl_Tests, ArrayIsConstexpr) {
    using namespace Sgcl;
    constexpr Array<int, 3> a = {1, 2, 3};
    static_assert(a.Count() == 3 && a[1] == 2 && a.First() == 1 && a.Last() == 3);
    static_assert(a.Contains(3) && !a.Contains(4) && a.IndexOf(2) == 1);
    static_assert(sorted_last() == 4);
    EXPECT_EQ(a.Count(), 3u);
}

// The rules the reviews added, seen through the wrappers: a List grows
// geometrically on Resize; the null end of a LinkedList that never held an
// element is an empty range to remove from; MoveToLast of the newest entry
// changes nothing; an ExpiryQueue assigned over releases the entries it held
TEST(Sgcl_Tests, TheRulesAfterTheReviews) {
    using namespace Sgcl;
    List<int> v(20000);
    size_t reallocations = 0;
    auto data = v.Data();
    for (int i = 0; i < 2000; ++i) {
        v.Resize(v.Count() + 1);
        if (v.Data() != data) {
            ++reallocations;
            data = v.Data();
        }
    }
    EXPECT_LE(reallocations, 2u);                           // geometric, not one per Resize
    EXPECT_EQ(v.Count(), 22000u);

    LinkedList<int> l;
    auto stale = end(l);                                    // null: no sentinel yet
    l.AddLast(1);
    l.AddLast(2);
    EXPECT_NE(stale, end(l));                               // invalidated by the first insertion
    l.RemoveRange(stale, end(l));                           // a range from the stale end: empty
    EXPECT_EQ(l.Count(), 2u);
    l.AddBefore(stale, 3);                                  // as a position: the end
    EXPECT_EQ(l.Last(), 3);

    OrderedDictionary<int, int> d = {{1, 10}, {2, 20}, {3, 30}};
    auto last = d.FindEntry(3);
    d.MoveToLast(last);                                     // already last: left alone
    EXPECT_EQ(last->first, 3);
    EXPECT_EQ(d.Last().first, 3);
    d.MoveToLast(d.FindEntry(1));
    EXPECT_EQ(d.Last().first, 1);
}
TEST(Sgcl_Tests, ArrayLinkedListDequeQueueStack) {
    using namespace Sgcl;
    Array<int, 3> a = {1, 2, 3};
    EXPECT_EQ(a.Count(), 3u);
    EXPECT_EQ(a.Last(), 3);
    Array<int> d(4);
    d.Fill(7);
    EXPECT_EQ(d[3], 7);
    LinkedList<int> ll = {2, 3};
    ll.AddFirst(1);
    ll.AddLast(4);
    EXPECT_EQ(ll.Count(), 4u);
    EXPECT_EQ(ll.Remove(3), 1u);
    LinkedList<int> other = {9};
    ll.Append(std::move(other));
    EXPECT_EQ(ll.Last(), 9);
    EXPECT_TRUE(other.IsEmpty());
    ll.Sort();
    ll.Reverse();
    EXPECT_EQ(ll.First(), 9);
    ForwardList<int> f = {1, 2, 3};
    f.AddFirst(0);
    f.Reverse();
    EXPECT_EQ(f.First(), 3);
    Deque<int> dq = {2};
    dq.AddFirst(1);
    dq.AddLast(3);
    EXPECT_EQ(dq[0], 1);
    EXPECT_EQ(dq.IndexOf(3), 2u);
    dq.RemoveFirst();
    EXPECT_EQ(dq.First(), 2);
    Queue<int> q = {1, 2};
    q.Enqueue(3);
    EXPECT_EQ(q.Dequeue(), 1);
    EXPECT_EQ(q.Peek(), 2);
    PriorityQueue<int> pq = {1, 5, 3};
    EXPECT_EQ(pq.Dequeue(), 5);
    EXPECT_EQ(pq.Peek(), 3);
    Stack<int> st;
    st.Push(1);
    st.Push(2);
    EXPECT_EQ(st.Pop(), 2);
    EXPECT_EQ(st.Peek(), 1);
    Queue<Ptr<Node>> qp;
    qp.Enqueue(Make<Node>(3));
    EXPECT_EQ(qp.Dequeue()->value, 3);
}

TEST(Sgcl_Tests, DictionariesAndSets) {
    using namespace Sgcl;
    Dictionary<String, int> d = {{"a", 1}};
    EXPECT_TRUE(d.Add("b", 2));
    EXPECT_FALSE(d.Add("a", 9));
    EXPECT_EQ(*d.Find("a"), 1);
    EXPECT_EQ(d.Find("z"), nullptr);
    d.Set("a", 5);
    d["c"] = 3;
    EXPECT_TRUE(d.ContainsKey("c"));
    EXPECT_TRUE(d.Remove("b"));
    EXPECT_FALSE(d.Remove("b"));
    int sum = 0;
    for (auto& [k, v] : d) {
        sum += v;
    }
    EXPECT_EQ(sum, 8);
    EXPECT_EQ(d.RemoveAll([](auto& p) { return p.second > 4; }), 1u);
    d.Set("x", 10);
    EXPECT_EQ(*d.Take("x"), 10);                           // the value out, the entry gone
    EXPECT_EQ(d.Take("x"), None);
    EXPECT_EQ(d.Take(std::string_view("nope")), None);
    SortedDictionary<String, int> taken = {{"k", 1}};
    EXPECT_EQ(*taken.Take("k"), 1);
    EXPECT_TRUE(taken.IsEmpty());
    OrderedDictionary<String, int> taken2 = {{"k", 2}};
    EXPECT_EQ(*taken2.Take(std::string_view("k")), 2);
    MultiDictionary<int, int> md;
    md.Add(1, 10);
    md.Add(1, 11);
    md.Add(2, 20);
    EXPECT_EQ(md.CountOf(1), 2u);
    EXPECT_EQ(md.Values(1).Count(), 2u);
    for (auto& [k, v] : md.Values(2)) {
        EXPECT_EQ(v, 20);
    }
    EXPECT_EQ(md.Remove(1), 2u);
    SortedDictionary<int, String> sd = {{3, "c"}, {1, "a"}};
    EXPECT_EQ(sd.First().first, 1);
    EXPECT_EQ(sd.Last().second, "c");
    EXPECT_TRUE(sd.Add(2, "b"));
    int prev = 0;
    for (auto& [k, v] : sd) {
        EXPECT_GT(k, prev);
        prev = k;
    }
    HashSet<int> hs = {1, 2};
    EXPECT_TRUE(hs.Add(3));
    EXPECT_FALSE(hs.Add(3));
    EXPECT_TRUE(hs.Remove(2));
    EXPECT_EQ(hs.Count(), 2u);
    HashMultiSet<int> hms;
    hms.Add(1);
    hms.Add(1);
    EXPECT_EQ(hms.CountOf(1), 2u);
    SortedSet<int> ss = {3, 1, 2};
    EXPECT_EQ(ss.First(), 1);
    EXPECT_EQ(ss.Last(), 3);
    SortedMultiSet<int> sms = {2, 2, 1};
    EXPECT_EQ(sms.CountOf(2), 2u);
    OrderedDictionary<String, int> od = {{"c", 1}, {"a", 2}};
    od.Set("b", 3);
    od.Set("a", 9);                                // present: stays where it was
    std::string order;
    for (auto& [k, v] : od) {
        order += k.CStr();
    }
    EXPECT_EQ(order, "cab");
    EXPECT_EQ(od.First().first, "c");
    EXPECT_EQ(od.Last().first, "b");
    EXPECT_TRUE(od.MoveToLast("c"));
    EXPECT_FALSE(od.MoveToLast("zz"));
    od.MoveToFirst(od.FindEntry(std::string_view("b")));
    EXPECT_EQ(od.First().first, "b");
    EXPECT_EQ(od.Last().first, "c");
    EXPECT_TRUE(od.RemoveFirst());
    EXPECT_EQ(od.First().first, "a");
    EXPECT_TRUE(od.RemoveLast());
    EXPECT_EQ(od.Count(), 1u);
    OrderedSet<int> os = {3, 1, 2};
    EXPECT_FALSE(os.Add(1));
    EXPECT_EQ(os.First(), 3);
    EXPECT_EQ(os.Last(), 2);
    os.MoveToFirst(2);
    EXPECT_EQ(os.First(), 2);
    EXPECT_TRUE(os.RemoveLast());
    EXPECT_EQ(os.Last(), 3);
    Dictionary<int, Ptr<Node>> dp;
    dp.Add(1, Make<Node>(7));
    EXPECT_EQ((*dp.Find(1))->value, 7);
    HashSet<Ptr<Node>> ps;
    Ptr p = Make<Node>();
    EXPECT_TRUE(ps.Add(p));
    EXPECT_TRUE(ps.Contains(p));
}

TEST(Sgcl_Tests, ConcurrentContainers) {
    using namespace Sgcl;
    ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    EXPECT_EQ(q.Dequeue(), 1);
    EXPECT_EQ(*q.TryDequeue(), 2);
    EXPECT_FALSE(q.TryDequeue());
    ConcurrentStack<Ptr<Node>> s;
    s.Push(Make<Node>(3));
    EXPECT_EQ(s.Pop()->value, 3);
    EXPECT_FALSE(s.TryPop());
    ConcurrentDictionary<int, int> d;
    std::thread t([&] {
        for (int i = 0; i < 1000; ++i) {
            d.Add(i, i);
        }
    });
    for (int i = 0; i < 1000; ++i) {
        d.Add(i, i);
    }
    t.join();
    EXPECT_EQ(d.Count(), 1000u);
    EXPECT_EQ(*d.TryGet(5), 5);
    EXPECT_FALSE(d.TryGet(5000));
    EXPECT_EQ(d.Find(7)->second, 7);
    EXPECT_EQ(d.Find(7000), d.End());
    EXPECT_TRUE(d.Remove(7));
    int n = 0;
    for (auto& [k, v] : d) {
        ++n;
    }
    EXPECT_EQ(n, 999);
    ConcurrentDictionary<String, int> ds = {{"alice", 30}};
    ConcurrentSortedDictionary<String, int> dss = {{"alice", 30}};
    ConcurrentHashSet<String> chs = {"alice"};
    ConcurrentSortedSet<String> css = {"alice"};
    std::string_view view = "alice";                  // a view converts to no String: the lookups are transparent
    EXPECT_EQ(ds.Find(view)->second, 30);
    EXPECT_EQ(*ds.TryGet(view), 30);
    EXPECT_EQ(dss.Find(view)->second, 30);
    EXPECT_EQ(dss.LowerBound(std::string_view("a"))->first, "alice");
    EXPECT_TRUE(ds.ContainsKey(view) && dss.ContainsKey(view) && chs.Contains(view) && css.Contains(view));
    EXPECT_TRUE(chs.FindEntry(view) != chs.End() && css.UpperBound(view) == css.End());
    EXPECT_TRUE(ds.Remove(view) && !ds.Remove(view) && dss.Remove(view) && chs.Remove(view) && css.Remove(view));
    ConcurrentSortedDictionary<int, int> sd;
    sd.Add(2, 2);
    sd.Add(1, 1);
    EXPECT_EQ(begin(sd)->first, 1);
    ConcurrentHashSet<int> hs;
    EXPECT_TRUE(hs.Add(1));
    EXPECT_FALSE(hs.Add(1));
    EXPECT_TRUE(hs.Remove(1));
    ConcurrentSortedSet<int> ss = {3, 1};
    EXPECT_EQ(*begin(ss), 1);
}

TEST(Sgcl_Tests, CopyOnWriteExpiryQueueAndWeakDictionary) {
    using namespace Sgcl;
    CopyOnWrite<List<int>> cfg(List<int>{1});
    auto snap = cfg.Load();
    cfg.Update([](List<int>& l) { l.Add(2); });
    EXPECT_EQ(cfg.Load()->Count(), 2u);
    EXPECT_EQ(snap->Count(), 1u);
    auto current = cfg.Load();
    EXPECT_TRUE(cfg.CompareExchange(current, List<int>{}));
    EXPECT_TRUE(cfg.Load()->IsEmpty());
    ExpiryQueue<Node> eq;
    int expired = 0;
    off_frame([&] {
        Ptr n = Make<Node>(4);
        auto e = eq.Watch(n, [&](Ptr<Node> p) { expired += p->value; });
        EXPECT_TRUE(e);
        EXPECT_FALSE(e.IsExpired());
    });
    settle();
    eq.Drain();
    EXPECT_EQ(expired, 4);
    WeakDictionary<Node, int> wd;
    WeakHashSet<Node> ws;
    off_frame([&] {                      // the key made and dropped in a frame below
        Ptr k = Make<Node>();
        EXPECT_TRUE(wd.Add(k, 1));
        EXPECT_FALSE(wd.Add(k, 2));
        wd[k] = 5;
        EXPECT_EQ(*wd.Find(k), 5);
        for (auto [w, v] : wd) {
            EXPECT_EQ(v, 5);
        }
        EXPECT_TRUE(ws.Add(k));
        EXPECT_TRUE(ws.Contains(k));
    });
    settle();
    EXPECT_EQ(wd.Sweep(), 1u);
    EXPECT_EQ(ws.Sweep(), 1u);
    EXPECT_TRUE(wd.IsEmpty() && ws.IsEmpty());
}

// An Expected of other types converts as sgcl::expected does: the value or
// the error, whichever it holds; a bool is never made from has_value (LWG
// 3836: an error became a success holding false); an Expected is not a
// value for the constructor from one
TEST(Sgcl_Tests, ExpectedConvertsFromAnExpectedOfOtherTypes) {
    using namespace Sgcl;
    Expected<int, String> value = 7;
    Expected<int, String> error(Unexpect, "no");
    Expected<long, String> lv = value;
    Expected<long, String> le = error;
    EXPECT_TRUE(lv.HasValue());
    EXPECT_EQ(*lv, 7L);
    EXPECT_FALSE(le.HasValue());
    EXPECT_EQ(le.Error(), "no");
    Expected<bool, String> bv(value);                       // explicit: bool is not convertible from int implicitly? it is; the value converted, not has_value
    Expected<bool, String> be(error);
    EXPECT_TRUE(bv.HasValue());
    EXPECT_TRUE(*bv);
    EXPECT_FALSE(be.HasValue());
    EXPECT_EQ(be.Error(), "no");
    Expected<bool, String> bz(Expected<int, String>(0));
    EXPECT_TRUE(bz.HasValue());
    EXPECT_FALSE(*bz);
    Expected<long, String> mv = std::move(value);
    EXPECT_EQ(*mv, 7L);
    static_assert(std::is_convertible_v<Expected<int, String>, Expected<long, String>>);
    static_assert(!std::is_convertible_v<Expected<int, String>, Expected<Ptr<Node>, String>>);
}

// A Function or MoveOnlyFunction made from an empty one of another
// signature is empty, as sgcl's are (the wrapper was handed over as a
// callable holding an empty function, and a call reached the empty one)
TEST(Sgcl_Tests, AFunctionFromAnEmptyOneOfAnotherSignatureIsEmpty) {
    using namespace Sgcl;
    Function<int(int)> none;
    Function<long(int)> from_none = none;
    EXPECT_FALSE(from_none);
    MoveOnlyFunction<long(int)> mo_from_none = none;
    EXPECT_FALSE(mo_from_none);
    MoveOnlyFunction<int(int)> mo_none;
    MoveOnlyFunction<long(int)> mo_from_mo_none = std::move(mo_none);
    EXPECT_FALSE(mo_from_mo_none);
    Function<int(int)> twice = [](int x) { return 2 * x; };
    Function<long(int)> from_twice = twice;
    ASSERT_TRUE(from_twice);
    EXPECT_EQ(from_twice(21), 42L);
    from_none = twice;
    EXPECT_EQ(from_none(4), 8L);
    from_none = none;
    EXPECT_FALSE(from_none);
    MoveOnlyFunction<long(int)> mo_from_twice = twice;
    ASSERT_TRUE(mo_from_twice);
    EXPECT_EQ(mo_from_twice(5), 10L);
}

TEST(Sgcl_Tests, AnyVariantFunctionExpected) {
    using namespace Sgcl;
    Any a = 5;
    EXPECT_TRUE(a.Is<int>());
    EXPECT_EQ(*a.Get<int>(), 5);
    EXPECT_EQ(a.Get<double>(), nullptr);
    a = Ptr(Make<Node>(3));
    EXPECT_EQ((*a.Get<Ptr<Node>>())->value, 3);
    Variant<int, Ptr<Node>, String> v = 1;
    EXPECT_EQ(v.Index(), 0u);
    v = Ptr(Make<Node>(2));
    EXPECT_TRUE(v.Is<Ptr<Node>>());
    EXPECT_EQ(v.Get<1>()->value, 2);
    v.Emplace<String>("s");
    EXPECT_EQ(v.Visit([](auto& x) -> size_t {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, String>) {
            return x.Length();
        } else {
            return 0;
        }
    }), 1u);
    Ptr n = Make<Node>(3);
    Function<int(int)> f = [n](int x) { return x + n->value; };
    EXPECT_EQ(f(1), 4);
    Function g = [](int x) { return x * 2; };
    EXPECT_EQ(g(2), 4);
    MoveOnlyFunction<int()> m = [u = Make<Node>(1), n]() { return u->value + n->value; };
    EXPECT_EQ(m(), 4);
    Dictionary<String, Any> bag;                 // an Any copied by a container: no conversion to sgcl::any tried
    bag.Set("n", 1);
    List<Function<int(int)>> handlers = {[](int x) { return x + 1; }};
    handlers.Add(handlers[0]);
    EXPECT_EQ(handlers[1](1), 2);
    List<Variant<int, String>> mixed = {1, String("s")};
    EXPECT_EQ(mixed.Count(), 2u);
    Expected<Ptr<Node>, String> ok = Ptr(Make<Node>(3));
    EXPECT_TRUE(ok);
    EXPECT_EQ((*ok)->value, 3);
    Expected<Ptr<Node>, String> bad = MakeUnexpected(String("no"));
    EXPECT_FALSE(bad.HasValue());
    EXPECT_EQ(bad.Error(), "no");
    EXPECT_EQ(bad.ValueOr(nullptr), nullptr);
    Expected<void, int> nok = Unexpected<int>(4);
    EXPECT_EQ(nok.Error(), 4);
}

namespace {
    using namespace Sgcl;

    Task<int> square(int x) {
        co_return x * x;
    }

    Task<int> sum(Channel<int>& ch) {
        int s = 0;
        while (auto v = co_await ch.AsyncReceive()) {
            s += *v;
        }
        co_return s;
    }

    Task<> producer(Channel<int>& ch, int n) {
        for (int i = 1; i <= n; ++i) {
            co_await ch.AsyncSend(i);
        }
        ch.Close();
    }

    // A Ptr local of a suspended frame is a root: the frame is managed
    Task<int> holds(Channel<void>& go) {
        Ptr n = Make<Node>(2);
        co_await go.AsyncReceive();
        co_await Yield();
        int a = co_await Spawn(square(3));
        co_return a + n->value;
    }

    Generator<int> count(int n) {
        for (int i = 0; i < n; ++i) {
            co_yield i;
        }
    }
}

TEST(Sgcl_Tests, TasksChannelsAndGenerators) {
    settle();
    const int before = Node::alive.load();
    EXPECT_EQ(Spawn(square(4)).Join(), 16);
    Channel<int> ch(4);
    auto s = Spawn(sum(ch));
    Go(producer(ch, 10));
    EXPECT_EQ(s.Join(), 55);
    Channel<void> go;
    auto h = Spawn(holds(go));
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);   // held by the waiting frame
    go.Send();
    EXPECT_EQ(h.Join(), 11);
    Channel<int> th;
    std::thread t([&] {
        for (int i = 0; i < 3; ++i) {
            th.Send(i);
        }
        th.Close();
    });
    int c = 0;
    for (int v : th) {
        c += v;
    }
    t.join();
    EXPECT_EQ(c, 3);
    EXPECT_TRUE(th.IsClosed());
    int g = 0;
    for (int v : count(4)) {
        g += v;
    }
    EXPECT_EQ(g, 6);
    Task<int> by_hand = square(5);
    by_hand.Resume();
    EXPECT_TRUE(by_hand.IsDone());
    EXPECT_EQ(by_hand.Result(), 25);
    EXPECT_GT(Scheduler::Workers(), 0u);
    EXPECT_FALSE(Scheduler::OnWorker());
    Scheduler::Stop();
    h.Destroy();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

// The members added for the parity with the sgcl pages: the tuple
// interface of Array, the iterator positions of the linked lists, the
// transparent lookups and the hash policy, the container parameter of
// the adapters, the monadic operations of Expected, Ptr<void>, ToShared
TEST(Sgcl_Tests, TheMirroredMembers) {
    using namespace Sgcl;
    Array<int, 3> a = {1, 2, 3};
    auto [x, y, z] = a;
    Get<1>(a) = 20;
    EXPECT_EQ(x + a[1] + ToArray({3, 2, 1})[0], 24);
    LinkedList<int> l = {1, 4};
    auto it = l.AddBefore(std::next(begin(l)), 2);
    l.AddAfter(it, 3);
    LinkedList<int> o = {9};
    l.Splice(begin(l), o, begin(o));
    EXPECT_EQ(l, LinkedList<int>({9, 1, 2, 3, 4}));
    EXPECT_EQ(l.RemoveAt(begin(l)), begin(l));
    ForwardList<int> f;
    f.AddAfter(f.BeforeBegin(), 1);
    f.EmplaceAfter(begin(f), 2);
    EXPECT_EQ(*f.RemoveAfter(f.BeforeBegin()), 2);
    struct StringHash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
    };
    Dictionary<std::string, int, StringHash, std::equal_to<>> d = {{"apple", 1}};
    EXPECT_TRUE(d.ContainsKey(std::string_view("apple")));
    EXPECT_EQ(d.FindEntry("apple")->second, 1);
    d.Reserve(100);
    EXPECT_EQ(d.BucketCount(), 128u);
    EXPECT_TRUE(d.KeyEqual()("a", "a"));
    SortedDictionary<int, int> sd = {{1, 1}, {5, 5}};
    EXPECT_EQ(sd.LowerBound(2)->first, 5);
    EXPECT_EQ(sd.EqualRange(5).Count(), 1u);
    SortedDictionary<int, int> greater = {{1, 2}};
    EXPECT_LT(sd, greater);
    Queue<int, LinkedList<int>> q;
    q.Enqueue(1);
    EXPECT_EQ(q.Dequeue(), 1);
    PriorityQueue<int, Deque<int>> pq(std::less<int>(), Deque<int>{2, 9, 4});
    EXPECT_EQ(pq.Peek(), 9);
    Stack<int, List<int>> st(List<int>{1, 2});
    EXPECT_EQ(st.Pop(), 2);
    ConcurrentDictionary<int, int> cd;
    EXPECT_EQ(cd.GetOrAdd(1, 5)->second, 5);
    EXPECT_EQ(cd.GetOrAdd(1, 9)->second, 5);
    Expected<int, String> e = 2;
    EXPECT_EQ(*e.Transform([](int v) { return v * 2; }), 4);
    EXPECT_EQ(e.AndThen([](int v) -> Expected<int, String> { return v + 1; }).Value(), 3);
    Expected<int, String> bad(Unexpect, "no");
    EXPECT_EQ(bad.OrElse([](const String&) -> Expected<int, String> { return 7; }).Value(), 7);
    EXPECT_EQ(bad.TransformError([](const String& s) { return s.Length(); }).Error(), 2u);
    try {
        bad.Value();
        FAIL();
    } catch (const BadExpectedAccess<String>& ex) {   // the String in the exception held through a root: alive while the exception is
        Collector::Collect(true);
        EXPECT_EQ(ex.Error(), "no");
    }
    Ptr n = Make<Node>(3);
    Ptr<void>& any = n;
    EXPECT_TRUE(any.Is<Node>());
    std::vector<std::shared_ptr<Node>> kept;                    // a std container: shared_ptr may live there
    kept.push_back(n.ToShared());
    EXPECT_EQ(kept[0]->value, 3);
    static_assert(sizeof(Ptr<Node>) == sizeof(void*));
    static_assert(sizeof(Variant<int, Ptr<Node>, WeakPtr<Node>>) == 16);   // the pointer words share one word
    List<Ptr<Node>> zeroed(4);                                  // a buffer of Ptrs: zeroed, not constructed
    EXPECT_EQ(zeroed[3], nullptr);
    String s = "abc";
    EXPECT_EQ(s.Object(), String(s).Object());
    EXPECT_EQ(ToString(42) + ToString(true), "42true");
}

TEST(Sgcl_Tests, CollectorFacade) {
    using namespace Sgcl;
    Ptr n = Make<Node>();
    Collector::Collect(true);
    EXPECT_GT(Collector::LiveObjectCount(), 0u);
    EXPECT_GT(Collector::GetStatistics().Cycles, 0u);
    EXPECT_GT(Collector::CommittedMemory(), 0u);
    EXPECT_EQ(Collector::GetRetained(n.Get()).Objects, 1u);
    auto [guard, referrers] = Collector::GetReferrers(n.Get());
    EXPECT_FALSE(referrers.empty());
}

TEST(Sgcl_Tests, TheAlgorithmsAreMembersOfEverySequence) {
    List v = {3, 1, 2};
    Deque d = {3, 1, 2};
    LinkedList l = {3, 1, 2};
    ForwardList f = {3, 1, 2};
    Array<int, 3> a = {3, 1, 2};
    Array<int> da = {3, 1, 2};
    v.Sort(); d.Sort(); l.Sort(); f.Sort(); a.Sort(); da.Sort();
    EXPECT_TRUE(v.IsSorted() && d.IsSorted() && l.IsSorted() && f.IsSorted() && a.IsSorted() && da.IsSorted());
    v.Reverse(); d.Reverse(); l.Reverse(); f.Reverse(); a.Reverse(); da.Reverse();
    EXPECT_EQ(v.IndexOf(3), 0u);
    EXPECT_EQ(d.LastIndexOf(1), 2u);
    EXPECT_TRUE(l.Contains(2));
    EXPECT_EQ(f.FindIndex([](int x) { return x == 1; }), 2u);
    EXPECT_EQ(*a.Find([](int x) { return x < 3; }), 2);
    EXPECT_TRUE(da.Exists([](int x) { return x == 3; }));
    EXPECT_TRUE(v.All([](int x) { return x > 0; }));
    EXPECT_EQ(v.CountOf([](int x) { return x > 1; }), 2u);
    EXPECT_EQ(v.Min(), 1);
    EXPECT_EQ(v.Max(), 3);
    EXPECT_EQ(a.Min(), 1);
    int sum = 0;
    l.ForEach([&](int x) { sum += x; });
    EXPECT_EQ(sum, 6);
    v.Fill(7);
    EXPECT_TRUE(v.All([](int x) { return x == 7; }));
    EXPECT_EQ(v.IndexOf(9), NoIndex);
    List sorted = {1, 3, 5, 7};                            // the lookups of a sorted sequence
    EXPECT_EQ(sorted.BinarySearch(5), 2u);
    EXPECT_EQ(sorted.BinarySearch(4), NoIndex);
    EXPECT_EQ(*sorted.LowerBound(4), 5);
    EXPECT_EQ(*sorted.UpperBound(5), 7);
    EXPECT_EQ(sorted.UpperBound(7), end(sorted));
    EXPECT_EQ(sorted.BinarySearch(3, std::less<int>()), 1u);
    auto& m = v;                                           // the mixin's reference: the list's own members, nothing else
    EXPECT_TRUE(m.Contains(7));
    static_assert(sizeof(List<int>) == 3 * sizeof(void*) && sizeof(Array<int, 3>) == 3 * sizeof(int));
}

TEST(Sgcl_Tests, TheDeductionGuides) {
    Atomic current = Make<Node>();
    static_assert(std::is_same_v<decltype(current), Atomic<Ptr<Node>>>);
    Atomic n = 5;
    static_assert(std::is_same_v<decltype(n), Atomic<int>>);
    Atomic host = String("x");
    static_assert(std::is_same_v<decltype(host), Atomic<String>>);
    List v = {1, 2};
    Queue q(v);
    Stack s(v);
    PriorityQueue pq(std::less<int>(), v);
    static_assert(std::is_same_v<decltype(q), Queue<int, List<int>>> && std::is_same_v<decltype(s), Stack<int, List<int>>>);
    EXPECT_EQ(pq.Peek(), 2);
    SortedDictionary sd = {Pair{1, 2}};
    Dictionary d = {Pair{1, 2}};
    SortedSet ss = {1, 2};
    HashSet hs = {1, 2};
    static_assert(std::is_same_v<decltype(sd), SortedDictionary<int, int>> && std::is_same_v<decltype(d), Dictionary<int, int>>);
    static_assert(std::is_same_v<decltype(ss), SortedSet<int>> && std::is_same_v<decltype(hs), HashSet<int>>);
    CopyOnWrite cow(std::string("x"));
    static_assert(std::is_same_v<decltype(cow), CopyOnWrite<std::string>>);
    MoveOnlyFunction f = [](int x) { return x + 1; };
    static_assert(std::is_same_v<decltype(f), MoveOnlyFunction<int(int)>>);
    EXPECT_EQ(f(1), 2);
    Thread t([] {});
    t.Join();
    EXPECT_FALSE(t.IsJoinable());
}

TEST(Sgcl_Tests, SelectOverChannels) {
    Channel<int> jobs(4);
    Channel<void> stop;
    Thread producer([&] {
        for (int i = 0; i < 200; ++i) {
            jobs.Send(i);
        }
        stop.Send();
    });
    long sum = 0;
    bool running = true;
    while (running) {
        Select(jobs.OnReceive([&](int j) { sum += j; }), stop.OnReceive([&] { running = false; }));
    }
    producer.Join();
    while (auto j = jobs.TryReceive()) {
        sum += *j;
    }
    EXPECT_EQ(sum, 199L * 200 / 2);
    Channel<int> out(1);
    int idle = 0;
    EXPECT_EQ(Select(out.OnSend(1), Otherwise([&] { ++idle; })), 0u);
    EXPECT_EQ(Select(out.OnSend(2), Otherwise([&] { ++idle; })), 1u);
    EXPECT_EQ(idle, 1);
    Channel<int> a(1), b(1);
    Task t = Spawn([](Channel<int>& a, Channel<int>& b) -> Task<int> {
        int total = 0;
        for (int n = 0; n < 2; ++n) {
            co_await AsyncSelect(a.OnReceive([&](int x) { total += x; }), b.OnReceive([&](int x) { total += 10 * x; }));
        }
        co_return total;
    }(a, b));
    a.Send(1);
    b.Send(2);
    EXPECT_EQ(t.Join(), 21);
    Scheduler::Stop();   // the workers joined and the queues gone: the tests after count live objects from zero
}

TEST(Sgcl_Tests, TimeSleepAfterTickTimeout) {
    using namespace std::chrono_literals;
    Task t = Spawn([]() -> Task<int> {
        co_await Sleep(10ms);
        co_return 1;
    }());
    EXPECT_EQ(t.Join(), 1);
    Ptr after = After(10ms);
    EXPECT_TRUE(after->Receive());
    EXPECT_FALSE(after->Receive());
    Channel<int> never;
    bool timed = false;
    EXPECT_EQ(Select(never.OnReceive([](int) {}), Timeout(10ms, [&] { timed = true; })), 1u);
    EXPECT_TRUE(timed);
    Ptr tick = Tick(3ms);
    int n = 0;
    while (n < 3) {
        if (tick->Receive()) {
            ++n;
        }
    }
    tick->Close();
    Scheduler::Stop();
}

TEST(Sgcl_Tests, StopSourceAndToken) {
    using namespace std::chrono_literals;
    StopSource src;
    StopToken tok = src.Token();
    Channel<int> jobs(4);
    Task t = Spawn([](Channel<int>& jobs, StopToken tok) -> Task<int> {
        int n = 0;
        bool running = true;
        while (running) {
            co_await AsyncSelect(jobs.OnReceive([&](int) { ++n; }), tok.OnStop([&] { running = false; }));
        }
        co_return n;
    }(jobs, tok));
    jobs.Send(1);
    jobs.Send(2);
    std::this_thread::sleep_for(20ms);
    src.RequestStop();
    EXPECT_EQ(t.Join(), 2);
    EXPECT_TRUE(tok.IsStopRequested());
    StopSource deadline;
    deadline.StopAfter(10ms);
    Select(deadline.Token().OnStop([] {}));
    EXPECT_TRUE(deadline.IsStopRequested());
    StopSource parent;
    StopSource child(parent.Token());
    parent.RequestStop();
    EXPECT_TRUE(child.IsStopRequested());
    Task w = Spawn([](StopToken tok) -> Task<int> {
        co_await tok.Stopped();
        co_return 7;
    }(child.Token()));
    EXPECT_EQ(w.Join(), 7);
    Scheduler::Stop();
}

TEST(Sgcl_Tests, WhenAllAndWhenAny) {
    using namespace std::chrono_literals;
    auto number = [](int n, int ms) -> Task<int> {
        co_await Sleep(std::chrono::milliseconds(ms));
        co_return n;
    };
    auto text = []() -> Task<std::string> { co_return "text"; };
    auto [n, s] = WhenAll(Spawn(number(1, 5)), Spawn(text())).Join();
    EXPECT_EQ(n, 1);
    EXPECT_EQ(s, "text");
    List<Task<int>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.Add(number(i, 4 - i));
    }
    List results = WhenAll(std::move(tasks)).Join();
    EXPECT_EQ(results.Count(), 4u);
    EXPECT_EQ(results[3], 3);
    EXPECT_EQ(WhenAny(number(1, 40), number(2, 2)).Join(), 1u);
    std::this_thread::sleep_for(60ms);
    Scheduler::Stop();
}

TEST(Sgcl_Tests, MutexSemaphoreEventWaitGroupOnce) {
    Mutex m;
    int shared = 0;
    List<Task<>> tasks;
    for (int i = 0; i < 4; ++i) {
        tasks.Add(Spawn([](Mutex& m, int& shared) -> Task<> {
            for (int k = 0; k < 500; ++k) {
                auto guard = co_await m.AsyncScopedLock();
                ++shared;
            }
        }(m, shared)));
    }
    Thread th([&] {
        for (int k = 0; k < 500; ++k) {
            std::lock_guard lock(m);
            ++shared;
        }
    });
    for (auto& t : tasks) {
        t.Join();
    }
    th.Join();
    EXPECT_EQ(shared, 2500);
    Semaphore sem(1);
    EXPECT_TRUE(sem.TryAcquire());
    EXPECT_FALSE(sem.TryAcquire());
    sem.Release();
    EXPECT_EQ(sem.Available(), 1u);
    Event ev;
    Task w = Spawn([](Event& ev) -> Task<int> {
        co_await ev.AsyncWait();
        co_return 1;
    }(ev));
    ev.Set();
    EXPECT_EQ(w.Join(), 1);
    EXPECT_TRUE(ev.IsSet());
    WaitGroup wg;
    std::atomic<int> n = {0};
    for (int i = 0; i < 5; ++i) {
        wg.Add();
        Go([](WaitGroup& wg, std::atomic<int>& n) -> Task<> {
            ++n;
            wg.Done();
            co_return;
        }(wg, n));
    }
    wg.Wait();
    EXPECT_EQ(n, 5);
    Once o;
    int inits = 0;
    Spawn(o.AsyncCall([](int& inits) -> Task<> {
        ++inits;
        co_return;
    }(inits))).Join();
    o.Call([&] { ++inits; });
    EXPECT_EQ(inits, 1);
    EXPECT_TRUE(o.IsCalled());
    Scheduler::Stop();
}

TEST(Sgcl_Tests, ReadableAndWritable) {
    using namespace std::chrono_literals;
    int fd[2];
    ASSERT_EQ(::pipe(fd), 0);
    Task t = Spawn([](int fd) -> Task<int> {
        co_await Readable(fd)->AsyncReceive();
        char c = 0;
        [[maybe_unused]] auto n = ::read(fd, &c, 1);
        co_return c;
    }(fd[0]));
    std::this_thread::sleep_for(10ms);
    [[maybe_unused]] auto n = ::write(fd[1], "x", 1);
    EXPECT_EQ(t.Join(), 'x');
    EXPECT_TRUE(Writable(fd[1])->Receive());
    ::close(fd[0]);
    ::close(fd[1]);
    Scheduler::Stop();
}

TEST(Sgcl_Tests, AsyncGeneratorAndSchedulerStatistics) {
    auto tens = [](Channel<int>& in) -> AsyncGenerator<int> {
        while (auto v = co_await in.AsyncReceive()) {
            co_yield *v * 10;
        }
    };
    Channel<int> in(2);
    Task t = Spawn([](Channel<int>& in, auto& tens) -> Task<int> {
        AsyncGenerator g = tens(in);
        int sum = 0;
        while (auto v = co_await g.Next()) {
            sum += *v;
        }
        co_return sum;
    }(in, tens));
    in.Send(1);
    in.Send(2);
    in.Close();
    EXPECT_EQ(t.Join(), 30);
    auto st = Scheduler::GetStatistics();
    EXPECT_EQ(st.Workers, Scheduler::Workers());
    Scheduler::Stop();
    EXPECT_EQ(Scheduler::GetStatistics().Workers, 0u);
}

TEST(Sgcl_Tests, StringBuilderBuildsAString) {
    StringBuilder b;
    b.Append("item ").Append(7).Append(": ").Append(String("ready"));
    b << " (" << 2.5 << ")";
    EXPECT_EQ(b.View(), "item 7: ready (2.500000)");
    String s = b.ToString();
    EXPECT_EQ(s, "item 7: ready (2.500000)");
    b.Clear();
    EXPECT_TRUE(b.IsEmpty());
    EXPECT_EQ(s.Length(), 24u);                        // the String independent of the buffer
    b.Append(3, 'x').Append('!');
    EXPECT_EQ(b.ToString(), "xxx!");
    EXPECT_EQ(b.Inner(), std::string("xxx!"));
}

TEST(Sgcl_Tests, RangeOverNumbersAndIterators) {
    int sum = 0;
    for (int i : Range(10)) {
        sum += i;
    }
    EXPECT_EQ(sum, 45);
    EXPECT_TRUE(Range(0).IsEmpty());
    EXPECT_EQ(Range(1).Count(), 1u);
    EXPECT_EQ(Range(1).First(), 0);
    List<int> seen;
    for (int i : Range(2, 10)) {
        seen.Add(i);
    }
    EXPECT_EQ(seen, (List<int>{2, 3, 4, 5, 6, 7, 8, 9}));
    EXPECT_EQ(Range(2, 10).Count(), 8u);
    EXPECT_TRUE(Range(5, 5).IsEmpty());
    EXPECT_TRUE(Range(7, 3).IsEmpty());                 // first past last: empty
    unsigned total = 0;
    for (unsigned i : Range(3u)) {
        total += i;
    }
    EXPECT_EQ(total, 3u);
    EXPECT_EQ(std::ranges::size(Range(10)), 10u);
    MultiDictionary<int, int> m = {{1, 10}, {1, 11}, {2, 20}};
    Range ones = m.Values(1);                            // the pair of iterators, as before
    EXPECT_EQ(ones.Count(), 2u);
    EXPECT_TRUE(ones.First().second == 10 || ones.First().second == 11);   // a hash multimap: either order
    EXPECT_EQ(ones.Inner().size(), 2u);                  // the sgcl::range inside
    EXPECT_TRUE(m.Values(3).IsEmpty());
}

TEST(Sgcl_Tests, SpawnAndGoTakeAClosure) {
    std::string big(1000, 'v');
    auto t = Sgcl::Spawn([big]() -> Sgcl::Task<size_t> {
        co_await Sgcl::Sleep(std::chrono::milliseconds(2));
        co_return big.size();
    });
    EXPECT_EQ(t.Join(), 1000u);
    std::atomic<int> seen = 0;
    Sgcl::Event done;
    Sgcl::Go([&seen, &done]() -> Sgcl::Task<> {
        seen = 1;
        done.Set();
        co_return;
    });
    done.Wait();
    EXPECT_EQ(seen.load(), 1);
    Sgcl::Scheduler::Stop();
}
