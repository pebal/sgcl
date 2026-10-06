//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <set>

// The collector learns where the pointers are in each type by elimination
// (detail/child_pointers.h); these tests pin down what that implies.
namespace {
    struct Payload {
        int v;
        Payload(int x) : v(x) { ++alive; }
        ~Payload() { v = -1; --alive; }
        inline static sgcl::atomic<int> alive = {0};
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

    // A root_ptr and a shared_ptr from to_shared() inside a managed object:
    // words naming a block of cells and a control block (which owns the
    // SharedHolder)
    struct Roots {
        root_ptr<Payload> root;
        std::shared_ptr<Payload> shared;
        tracked_ptr<Payload> tracked;
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
    // An owner and a raw word into what it owns (io::reader's handle,
    // slice): the raw word is data to the collector, and safe, since the
    // tracked word of the same object keeps its target
    struct Covered {
        Covered() {}
        tracked_ptr<Payload> owner;
        size_t raw = 0;
    };

    // The same shape with the owner pointing elsewhere: the raw word
    // keeps nothing, which is the mistake the report is for
    struct Uncovered {
        Uncovered() {}
        tracked_ptr<Payload> owner;
        size_t raw = 0;
    };

    // A plain object a word may point into: a pointer to a member is a
    // pointer to the object
    struct Blob {
        char bytes[256] = {};
    };

    // An owner and a raw word one past the end of what it owns (a slice's
    // end): the word names the next slot, not the owned object
    struct EndCovered {
        EndCovered() {}
        tracked_ptr<Blob> owner;
        size_t end = 0;
    };

    struct Packed {
        Packed() {}
        size_t word = 0;
    };

    struct PackedInArray {
        PackedInArray() {}
        size_t word = 0;
    };

    // A slot of a pool of its own, for the stepper scenarios below
    struct Slot {
        char bytes[256] = {};
    };

    // An owner and a raw word one past the end of what it owns, the owned
    // object made in the cycle being stepped: registered only by the next
    struct FreshEndCovered {
        FreshEndCovered() {}
        tracked_ptr<Slot> owner;
        size_t end = 0;
    };

    // The same shape with the owner pointing elsewhere: the raw word names
    // the start of an old object the holder keeps nothing of
    struct FreshEndUncovered {
        FreshEndUncovered() {}
        tracked_ptr<Slot> owner;
        size_t end = 0;
    };

#if !defined(NDEBUG)
    // Old Slots with holes before them: four pages' worth made, three of
    // four dropped, so that the next full cycles empty their pages by more
    // than half and hand them back to the allocator
    void make_old_slots(sgcl::vector<tracked_ptr<Slot>>& old) {
        off_frame([&] {
            old.reserve(1024);
            for (int i = 0; i < 1024; ++i) {
                old.push_back(make_tracked<Slot>());
            }
            for (int i = 0; i < 1024; ++i) {
                if (i % 4 != 0) {
                    old[i] = nullptr;
                }
            }
        });
    }

    // Two full cycles under the stepper: the dropped Slots swept, the
    // holders' maps built, the survivors registered and marked
    void settle(collector::stepper& s) {
        for (int i = 0; i < 2; ++i) {
            collector::clear_stack(SIZE_MAX);
            s.finish_cycle();
        }
    }

    // Slots made now until one lands in the slot right before an old one:
    // {the fresh one, the old one}, or nulls
    std::pair<tracked_ptr<Slot>, tracked_ptr<Slot>> fresh_before_old(sgcl::vector<tracked_ptr<Slot>>& old) {
        std::set<const char*> kept;
        for (size_t i = 0; i < old.size(); ++i) {
            if (old[i]) {
                kept.insert((const char*)old[i].get());
            }
        }
        for (int i = 0; i < 4096; ++i) {
            tracked_ptr<Slot> fresh = make_tracked<Slot>();
            auto after = (const char*)fresh.get() + sizeof(Slot);
            if (kept.count(after)) {
                for (size_t j = 0; j < old.size(); ++j) {
                    if ((const char*)old[j].get() == after) {
                        return {fresh, old[j]};
                    }
                }
            }
        }
        return {};
    }
#endif
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

// A word naming a root by state (the cell of a root_ptr, the holder of a
// to_shared) is data to the map, as a word naming unmanaged memory (a
// shared_ptr's control block) is: the offset leaves it as any data offset
// does, and the root keeps its object by its own state
TEST(Maps_Tests, WordsNamingRootsByStateAreData) {
    tracked_ptr<Roots> r = make_tracked<Roots>();
    off_frame([&] {
        r->root = make_tracked<Payload>(5);
        r->shared = tracked_ptr<Payload>(make_tracked<Payload>(6)).to_shared();
        r->tracked = make_tracked<Payload>(7);
    });
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    // the root_ptr's word (a cell of a block) and the control block's are
    // data; the shared_ptr's first word is the object's address, a raw
    // pointer to a managed object, kept as any raw pointer is
    std::vector<size_t> expected = {offsetof(Roots, shared) / sizeof(void*), offsetof(Roots, tracked) / sizeof(void*)};
    EXPECT_EQ(candidate_words<Roots>(), expected);
    off_frame([&] {                                  // the reads in a frame of their own: what they spill is cleared
        EXPECT_EQ(r->root->v, 5);
        EXPECT_EQ(r->shared->v, 6);
        EXPECT_EQ(r->tracked->v, 7);
    });
    EXPECT_EQ(Payload::alive.load(), 3);
    r = nullptr;
    collector::clear_stack(SIZE_MAX);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Payload::alive.load(), 0);   // the Roots died: its root_ptr and shared_ptr ran their destructors in the sweep
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

// A raw word whose target a tracked word of the same object holds is not
// reported: the owner keeps what the raw word names
TEST(Maps_Tests, ARawWordItsOwnerCoversIsNotReported) {
    tracked_ptr<Payload> target = make_tracked<Payload>(5);
    tracked_ptr<Covered> data = make_tracked<Covered>();
    tracked_ptr<Covered> covered = make_tracked<Covered>();
    data->raw = 12345;
    covered->owner = target;
    covered->raw = (size_t)target.get();
    ::testing::internal::CaptureStderr();
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(output.find("Covered"), std::string::npos) << output;
}

// The same with the owner elsewhere: reported, the raw word keeping nothing
TEST(Maps_Tests, ARawWordNoOwnerCoversIsReported) {
    tracked_ptr<Payload> target = make_tracked<Payload>(5);
    tracked_ptr<Uncovered> data = make_tracked<Uncovered>();
    tracked_ptr<Uncovered> uncovered = make_tracked<Uncovered>();
    data->raw = 12345;
    uncovered->owner = make_tracked<Payload>(6);
    uncovered->raw = (size_t)target.get();
    ::testing::internal::CaptureStderr();
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("Uncovered"), std::string::npos) << output;
}

// A raw word one past the end of an object a tracked word of the same
// object holds is not reported, though the address is the start of the
// next slot: a slice's end, which nothing reads through
TEST(Maps_Tests, AnEndWordItsOwnerCoversIsNotReported) {
#if defined(SGCL_ASAN)
    GTEST_SKIP() << "under the address sanitizer no object ends where the next slot starts: a redzone lies between (page_info.h: SlotSize)";
#endif
    tracked_ptr<Blob> first = make_tracked<Blob>();
    tracked_ptr<Blob> next;
    for (int i = 0; i < 64 && !next; ++i) {   // a Blob in the slot right after first's
        tracked_ptr<Blob> b = make_tracked<Blob>();
        if ((char*)b.get() == (char*)first.get() + sizeof(Blob)) {
            next = b;
        }
    }
    ASSERT_TRUE(next);
    tracked_ptr<EndCovered> data = make_tracked<EndCovered>();
    tracked_ptr<EndCovered> covered = make_tracked<EndCovered>();
    data->end = 12345;
    covered->owner = first;
    covered->end = (size_t)((char*)first.get() + sizeof(Blob));
    ::testing::internal::CaptureStderr();
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(output.find("EndCovered"), std::string::npos) << output;
}

// The same with the owned object made after the registration of the cycle
// that traces the holder (registered by the next one only): the word that
// holds it is the evidence, not the registration (the HTTP/2 owned slices,
// DESIGN 440)
TEST(Maps_Tests, AnEndWordOfAFreshOwnedObjectIsNotReported) {
#if defined(SGCL_ASAN)
    GTEST_SKIP() << "under the address sanitizer no object ends where the next slot starts: a redzone lies between (page_info.h: SlotSize)";
#endif
    collector::stepper s;
    sgcl::vector<tracked_ptr<Slot>> old;
    tracked_ptr<FreshEndCovered> data = make_tracked<FreshEndCovered>();
    tracked_ptr<FreshEndCovered> covered = make_tracked<FreshEndCovered>();
    data->end = 12345;
    make_old_slots(old);
    settle(s);
    s.advance_to(collector::stepper::phase::registered);
    auto [first, next] = fresh_before_old(old);   // first: unregistered this cycle; next: old, registered
    ASSERT_TRUE(first);
    covered->owner = first;
    covered->end = (size_t)((char*)first.get() + sizeof(Slot));
    ::testing::internal::CaptureStderr();
    s.finish_cycle();
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(output.find("FreshEndCovered"), std::string::npos) << output;
}

// A raw word at the start of an old object whose predecessor, fresh, the
// holder does not name: reported, the raw word keeping nothing
TEST(Maps_Tests, AStartWordWhosePredecessorIsNotHeldIsReported) {
#if defined(SGCL_ASAN)
    GTEST_SKIP() << "under the address sanitizer no object ends where the next slot starts: a redzone lies between (page_info.h: SlotSize)";
#endif
    collector::stepper s;
    sgcl::vector<tracked_ptr<Slot>> old;
    tracked_ptr<FreshEndUncovered> data = make_tracked<FreshEndUncovered>();
    tracked_ptr<FreshEndUncovered> uncovered = make_tracked<FreshEndUncovered>();
    data->end = 12345;
    make_old_slots(old);
    settle(s);
    s.advance_to(collector::stepper::phase::registered);
    auto [first, next] = fresh_before_old(old);
    ASSERT_TRUE(first);
    uncovered->owner = make_tracked<Slot>();   // a Slot elsewhere
    uncovered->end = (size_t)next.get();
    ::testing::internal::CaptureStderr();
    s.finish_cycle();
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("FreshEndUncovered"), std::string::npos) << output;
}

// A raw word inside a plain object, not at its start, is reported: a
// pointer to a member keeps the object as one to its start would, and the
// raw word keeps nothing
TEST(Maps_Tests, AWordInsideAPlainObjectIsReported) {
    tracked_ptr<Blob> target = make_tracked<Blob>();
    tracked_ptr<Packed> data = make_tracked<Packed>();
    tracked_ptr<Packed> inside = make_tracked<Packed>();
    data->word = 12345;
    inside->word = (size_t)target.get() + 64;
    ::testing::internal::CaptureStderr();
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("Packed"), std::string::npos) << output;
}

// A raw word inside an array (a buffer), not at its start, is not: a
// pointer to an array names its start, an element is reached through it
TEST(Maps_Tests, AWordInsideAnArrayIsNotReported) {
    sgcl::vector<char> target(256);
    tracked_ptr<PackedInArray> data = make_tracked<PackedInArray>();
    tracked_ptr<PackedInArray> inside = make_tracked<PackedInArray>();
    data->word = 12345;
    inside->word = (size_t)target.data() + 64;
    ::testing::internal::CaptureStderr();
    for (int i = 0; i < 4; ++i) {
        collector::force_collect(true);
    }
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(output.find("PackedInArray"), std::string::npos) << output;
}
#endif
