//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// copy_from reads through io's copy block on its stack (the audit of the
// managed heap, 2026-09-26): no managed block of 32 KB lives while the
// stream is read. async_copy_from's reads may run on the pool, so its block
// is managed and the slice a read is given holds it.
#include "common.h"

using namespace sgcl::async;

#include <atomic>
#include <cstring>
#include <typeinfo>

namespace {
    namespace io = sgcl::io;

    size_t live_of(const std::type_info& type) {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (!s.buffers && *s.type == type) {
                n += s.live_objects;
            }
        }
        return n;
    }

    using ManagedCopyBlock = sgcl::array<std::byte, 32768>;

    // A stream of `size` bytes that counts the managed copy blocks at its second read
    struct Looking {
        size_t size;
        size_t done = 0;
        size_t calls = 0;
        size_t blocks = size_t(-1);

        expected<size_t, io::error> read(const sgcl::slice<byte>& out) {
            if (++calls == 2) {
                blocks = live_of(typeid(ManagedCopyBlock));
            }
            size_t k = std::min(out.size(), size - done);
            std::memset(out.data(), 'x', k);
            done += k;
            return k;
        }

        task<expected<size_t, io::error>> async_read(sgcl::slice<byte> out) {
            co_return read(out);
        }
    };

    // A stream with no async_read of its own: a task's read of it runs on
    // the pool. Whether each slice it was given held an owner.
    struct PoolReader {
        size_t size;
        size_t done = 0;
        std::atomic<bool> owned = true;

        expected<size_t, io::error> read(const sgcl::slice<byte>& out) {
            if (!out.owner()) {
                owned = false;
            }
            size_t k = std::min(out.size(), size - done);
            std::memset(out.data(), 'x', k);
            done += k;
            return k;
        }
    };
}

TEST(HashHeap_Tests, CopyFromMakesNoManagedBlock) {
    // blocks an earlier test's tasks left may still be alive (a word of a
    // pool thread's stack), and may be swept meanwhile: none is added
    size_t before = live_of(typeid(ManagedCopyBlock));
    Looking r{1 << 20};
    sgcl::hash::crc32 h;
    auto n = h.copy_from(io::reader(r));
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, size_t(1) << 20);
    EXPECT_LE(r.blocks, before);
}

TEST(HashHeap_Tests, AsyncCopyFromHandsThePoolAnOwnedBlock) {
    PoolReader r{1 << 20};
    auto run = [&]() -> task<size_t> {
        sgcl::hash::crc32 h;
        auto n = co_await h.async_copy_from(io::reader(r));
        co_return n ? *n : 0;
    };
    EXPECT_EQ(spawn(run()).wait(), size_t(1) << 20);
    EXPECT_TRUE(r.owned);
}
