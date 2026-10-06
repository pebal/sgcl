//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

// The managed heap under the address sanitizer (os.h: SGCL_ASAN): a read
// one byte past a managed object, past a buffer (one that ends inside its
// size class and one that fills the class exactly), past a string, past a
// buffer of pages, and a read of a swept object through a raw pointer kept
// aside are each reported, as they would be in malloc's memory. Each read
// runs in a child process (a death test, re-executed: the collector's
// thread does not survive a fork); in a build without the sanitizer the
// cases are skipped.
#if defined(SGCL_ASAN)
namespace {
    struct Odd {
        int v[5];   // 20 bytes: the object ends inside a granule of the sanitizer
    };

    struct Wide {
        tracked_ptr<Wide> next;
        long data[3];
    };

    struct alignas(32) Aligned {
        char c[32];
    };

    // A read the compiler keeps, through a pointer it cannot see into
    SGCL_NOINLINE char read_byte(const void* p) {
        return *(const volatile char*)p;
    }

    class AsanHeap : public ::testing::Test {
    protected:
        void SetUp() override {
            GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
        }
    };
}

TEST_F(AsanHeap, TheObjectItselfIsReadable) {
    auto o = make_tracked<Odd>();
    auto w = make_tracked<Wide>();
    auto a = make_tracked<Aligned>();
    for (size_t i = 0; i < sizeof(Odd); ++i) {
        read_byte((const char*)o.get() + i);
    }
    for (size_t i = 0; i < sizeof(Wide); ++i) {
        read_byte((const char*)w.get() + i);
    }
    EXPECT_EQ((uintptr_t)a.get() % alignof(Aligned), 0u);
    for (size_t i = 0; i < sizeof(Aligned); ++i) {
        read_byte(a->c + i);
    }
}

TEST_F(AsanHeap, OneBytePastAnObject) {
    EXPECT_DEATH({
        auto o = make_tracked<Odd>();
        read_byte((const char*)o.get() + sizeof(Odd));
    }, "heap-buffer-overflow");
    EXPECT_DEATH({
        auto w = make_tracked<Wide>();
        read_byte((const char*)w.get() + sizeof(Wide));
    }, "heap-buffer-overflow");
    EXPECT_DEATH({
        auto a = make_tracked<Aligned>();
        read_byte(a->c + sizeof(Aligned));
    }, "heap-buffer-overflow");
}

TEST_F(AsanHeap, OneBytePastAnObjectsNeighbourIsNotItsSlot) {
    // two objects made one after another: the end of the first is not the
    // start of the second
    auto a = make_tracked<Odd>();
    auto b = make_tracked<Odd>();
    EXPECT_NE((const char*)a.get() + sizeof(Odd), (const char*)b.get());
    EXPECT_NE((const char*)b.get() + sizeof(Odd), (const char*)a.get());
}

TEST_F(AsanHeap, PastABufferEndingInsideItsClass) {
    vector<char> v(13, 'x');   // the class of 32 bytes holds it
    for (size_t i = 0; i < v.size(); ++i) {
        read_byte(v.data() + i);
    }
    EXPECT_EQ(v.capacity(), 13u);   // the capacity asked for, never the class's
    EXPECT_DEATH({
        vector<char> v(13, 'x');
        read_byte(v.data() + 13);
    }, "heap-buffer-overflow");
}

TEST_F(AsanHeap, PastABufferFillingItsClass) {
    // 48 bytes fill the class of 48 exactly: the buffer takes a class with
    // a redzone after it
    vector<char> v(48, 'x');
    for (size_t i = 0; i < v.size(); ++i) {
        read_byte(v.data() + i);
    }
    EXPECT_DEATH({
        vector<char> v(48, 'x');
        read_byte(v.data() + 48);
    }, "heap-buffer-overflow");
    EXPECT_DEATH({
        vector<long> v(6, 1);   // 48 bytes of elements
        read_byte((const char*)v.data() + 48);
    }, "heap-buffer-overflow");
}

TEST_F(AsanHeap, PastAGrownVector) {
    vector<int> v;
    for (int i = 0; i < 1000; ++i) {
        v.push_back(i);
    }
    for (size_t i = 0; i < v.size(); ++i) {
        read_byte(v.data() + i);
    }
    EXPECT_DEATH({
        vector<int> v;
        for (int i = 0; i < 1000; ++i) {
            v.push_back(i);
        }
        read_byte((const char*)(v.data() + v.capacity()));
    }, "heap-buffer-overflow");
}

TEST_F(AsanHeap, PastAString) {
    // the characters and the terminator are readable, the byte after not:
    // a short string (a class of four bytes) and a long one (a class of
    // half again)
    string s("hello, world");
    for (size_t i = 0; i <= s.size(); ++i) {
        read_byte(s.c_str() + i);
    }
    EXPECT_DEATH({
        string s(std::string(300, 'y'));
        read_byte(s.c_str() + s.size() + 1);
    }, "heap-buffer-overflow");
    EXPECT_DEATH({
        string s("hello, world!");   // 8 + 14 bytes in the class of 24
        read_byte(s.c_str() + s.size() + 1);
    }, "heap-buffer-overflow");
}

TEST_F(AsanHeap, PastABufferOfPages) {
    vector<char> v(200000, 'z');
    read_byte(v.data());
    read_byte(v.data() + v.size() - 1);
    EXPECT_DEATH({
        vector<char> v(200000, 'z');
        read_byte(v.data() + v.size());
    }, "heap-buffer-overflow");
    EXPECT_DEATH({
        vector<char> v(3 * config::page_size - 16, 'z');   // the header and the elements fill three pages: a redzone on a fourth
        read_byte(v.data() + v.size());
    }, "heap-buffer-overflow");
}

TEST_F(AsanHeap, PastAnObjectOfAPage) {
    struct Big {
        char c[65000];   // one slot to a page
    };
    EXPECT_DEATH({
        auto b = make_tracked<Big>();
        read_byte(b->c + sizeof(Big));
    }, "heap-buffer-overflow");
}

namespace {
    // A raw pointer to a fresh object, hidden from the stack scan, the
    // object's only handle gone with the frame
    SGCL_NOINLINE uintptr_t make_garbage() {
        auto o = make_tracked<Odd>();
        o->v[0] = 42;
        return hide(o.get());
    }

    SGCL_NOINLINE uintptr_t make_garbage_buffer(size_t n) {
        vector<char> v(n, 'q');
        return hide(v.data() + n / 2);
    }
}

TEST_F(AsanHeap, ASweptObject) {
    EXPECT_DEATH({
        auto h = make_garbage();
        collector::clear_stack();
        collector::force_collect(true);
        collector::force_collect(true);
        read_byte(unhide(h));
    }, "heap-use-after-free");
    EXPECT_DEATH({
        auto h = make_garbage_buffer(100);
        collector::clear_stack();
        collector::force_collect(true);
        collector::force_collect(true);
        read_byte(unhide(h));
    }, "heap-use-after-free");
    // a buffer of pages: its first megabyte (collector.h: _poison_slot)
    EXPECT_DEATH({
        auto h = make_garbage_buffer(300000);
        collector::clear_stack();
        collector::force_collect(true);
        collector::force_collect(true);
        read_byte(unhide(h));
    }, "heap-use-after-free");
}

TEST_F(AsanHeap, TheRangeOfADeadBufferServesTheNext) {
    // the pages of a dead buffer of pages, given back to the heap with
    // their shadow (os.h: asan::release), hold the next buffers whole
    for (int round = 0; round < 4; ++round) {
        off_frame([] {
            vector<char> big(3'000'000, 'b');
            read_byte(big.data() + big.size() - 1);
        });
        collector::force_collect(true);
        vector<char> next(2'000'000 + round * 100'000, 'n');
        for (size_t i = 0; i < next.size(); i += 4096) {
            read_byte(next.data() + i);
        }
        read_byte(next.data() + next.size() - 1);
        vector<int> small(1000, 1);   // pool pages from the same heap
        read_byte(small.data() + 999);
    }
}

TEST_F(AsanHeap, ASlotReusedAfterTheSweepIsReadable) {
    // the slots the sweep poisoned come back unpoisoned to the next objects
    for (int round = 0; round < 3; ++round) {
        off_frame([] {
            for (int i = 0; i < 20000; ++i) {
                auto o = make_tracked<Odd>();
                o->v[4] = i;
            }
        });
        collector::force_collect(true);
    }
    auto o = make_tracked<Odd>();
    for (size_t i = 0; i < sizeof(Odd); ++i) {
        read_byte((const char*)o.get() + i);
    }
}
#else
TEST(AsanHeap, OnlyUnderTheAddressSanitizer) {
    GTEST_SKIP() << "built without the address sanitizer";
}
#endif
