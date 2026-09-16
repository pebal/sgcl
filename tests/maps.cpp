//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <cstring>

// The collector learns where the pointers are in each type by elimination
// (detail/child_pointers.h); these tests pin down what that implies.
namespace {
    struct Payload {
        int v;
        Payload(int x) : v(x) { ++alive; }
        ~Payload() { v = -1; --alive; }
        inline static gc::atomic<int> alive = {0};
    };

    struct Mixed {
        size_t count = 7;              // data, non-zero from the start
        double ratio = 1.5;            // data
        tracked_ptr<Payload> first;    // pointer
        char name[16] = "mixed-up-name!!";   // data, two words, both non-zero
        size_t zero = 0;               // always zero: never proven to be data
        tracked_ptr<Payload> second;   // pointer
    };

    template<class T>
    std::vector<size_t> candidate_words() {
        std::vector<size_t> result;
        auto& map = sgcl::detail::TypeInfo<T>::child_pointers().map;
        for (size_t w = 0; w < map.size(); ++w) {
            for (int b = 0; b < 64; ++b) {
                if (map[w] & (uint64_t(1) << b)) {
                    result.push_back(w * 64 + b);
                }
            }
        }
        return result;
    }

    struct Virtual {
        virtual ~Virtual() = default;
        tracked_ptr<Payload> p;
    };

    struct RawHolder {
        RawHolder() {}
        Payload* raw = nullptr;
    };

    // A destructor reads a tracked_ptr member through if_alive() only: null
    // for a peer dying in the same sweep, the object otherwise. An owning
    // unique_ptr it may use as it is: the owned object is alive until the
    // owner destroys it.
    struct Node {
        tracked_ptr<Node> peer;
        tracked_ptr<Payload> live;
        unique_ptr<Payload> owned;
        inline static int saw_null_peer = 0;
        inline static int saw_live = 0;
        inline static int saw_owned = 0;
        ~Node() {
            saw_null_peer += peer.if_alive() == nullptr;
            if (auto p = live.if_alive()) {
                saw_live += p->v == 9;
            }
            saw_owned += owned != nullptr && owned->v > 0;
        }
    };

    struct Holder {
        tracked_ptr<Payload> p;
        inline static int saw_alive = 0;
        ~Holder() {
            saw_alive += p.if_alive() != nullptr;
        }
    };

    struct Overlapping {
        Overlapping() {}
        size_t word = 0;   // data in one object, an address in another: rule 2 broken
    };
}

TEST(Maps_Tests, DataOffsetsAreEliminatedPointerOffsetsStay) {
    tracked_ptr<Mixed> keep[3];
    for (auto& k : keep) {
        k = make_tracked<Mixed>();
        k->first = make_tracked<Payload>(1);
        k->second = make_tracked<Payload>(2);
    }
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    std::vector<size_t> expected = {
        offsetof(Mixed, first) / sizeof(void*),
        offsetof(Mixed, zero) / sizeof(void*),
        offsetof(Mixed, second) / sizeof(void*),
    };
    EXPECT_EQ(candidate_words<Mixed>(), expected);
    // and the pointers are still followed after the elimination
    for (auto& k : keep) {
        EXPECT_EQ(k->first->v, 1);
        EXPECT_EQ(k->second->v, 2);
    }
    EXPECT_EQ(Payload::alive.load(), 6);
}

TEST(Maps_Tests, PointerBehindAVtable) {
    tracked_ptr<Virtual> v = make_tracked<Virtual>();
    v->p = make_tracked<Payload>(4);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(v->p->v, 4);
    auto offset = size_t((char*)&v->p - (char*)v.get()) / sizeof(void*);   // offsetof: not standard-layout
    EXPECT_EQ(candidate_words<Virtual>(), std::vector<size_t>{offset});
}

// A raw pointer inside a managed object is not a reference: the target must
// be kept by a tracked_ptr. While it is, the raw pointer is usable; once the
// tracked_ptr is gone the target may be collected in any cycle.
TEST(Maps_Tests, RawPointerIsNotAReference) {
    tracked_ptr<RawHolder> h = make_tracked<RawHolder>();
    tracked_ptr<Payload> keeper = make_tracked<Payload>(3);
    off_frame([&] {
        h->raw = keeper.get();     // the raw value must not linger in the test's frame
    });
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Payload::alive.load(), 1);
    off_frame([&] {
        EXPECT_EQ(h->raw->v, 3);   // kept by `keeper`, not by `raw`
    });
    keeper = nullptr;
    h = nullptr;
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Payload::alive.load(), 0);
}

// Objects dying together, cross-linked: in the destructor if_alive() is
// null for the peer (dying too) and the survivor for the live pointer;
// each owner finds its owned object intact and destroys it exactly once.
TEST(Maps_Tests, DestructorSeesDyingPeersAsNullThroughIfAlive) {
    Node::saw_null_peer = Node::saw_live = Node::saw_owned = 0;
    tracked_ptr<Payload> survivor = make_tracked<Payload>(9);
    off_frame([&] {
        tracked_ptr<Node> a = make_tracked<Node>();
        tracked_ptr<Node> b = make_tracked<Node>();
        a->peer = b;
        b->peer = a;
        a->live = survivor;
        b->live = survivor;
        a->owned = make_tracked<Payload>(10);
        b->owned = make_tracked<Payload>(11);
    });
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Node::saw_null_peer, 2);
    EXPECT_EQ(Node::saw_live, 2);
    EXPECT_EQ(Node::saw_owned, 2);
    EXPECT_EQ(Payload::alive.load(), 1);   // only the survivor
    EXPECT_EQ(survivor->v, 9);
}

// Outside a sweep (a stack object, an explicit erase) if_alive() is the
// pointer: a live object's targets are live, whatever the collector is
// doing meanwhile.
TEST(Maps_Tests, IfAliveIsThePointerOutsideASweep) {
    Holder::saw_alive = 0;
    tracked_ptr<Payload> target = make_tracked<Payload>(1);
    {
        Holder on_stack;
        on_stack.p = target;
    }
    sgcl::list<Holder> in_list;
    in_list.emplace_back().p = target;
    in_list.clear();
    EXPECT_EQ(Holder::saw_alive, 2);
}

#if !defined(NDEBUG)
// Rule 2 broken on purpose: the same word is data in one object and holds a
// managed address in another. Debug builds report it once per type.
TEST(Maps_Tests, SharedStorageIsReportedInDebug) {
    tracked_ptr<Payload> target = make_tracked<Payload>(5);
    tracked_ptr<Overlapping> data = make_tracked<Overlapping>();
    tracked_ptr<Overlapping> address = make_tracked<Overlapping>();
    data->word = 12345;
    address->word = (size_t)target.get();
    ::testing::internal::CaptureStderr();
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("classified as data but holds a pointer"), std::string::npos) << output;
}
#endif
