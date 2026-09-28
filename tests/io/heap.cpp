//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io: what an operation leaves to the collector (the audit of the managed
// heap, 2026-09-26). Scratch a call keeps to itself is unmanaged: copy's
// block on the stack, read_all's growth in plain memory with one vector of
// exactly the size made at the end, read_file without a stat's name or a
// block for the probe past the end, a buffered_writer's block its own while
// it is written from its thread. A block an async read or write is handed
// is managed, since the operation may run on the pool and outlive the
// frame of a task let go of: the slice it is given holds its owner. Each
// case counts the live objects of the type it no longer makes, while the
// operation runs (the reader's callback) or while the object lives, or
// looks at the slice an operation on the pool is given.
#include "tests/types.h"

#include <atomic>
#include <cstring>
#include <string>
#include <typeinfo>

using namespace sgcl::async;

namespace {
    namespace io = sgcl::io;

    // The live objects (or buffers) of a type, after a full cycle
    size_t live_of(const std::type_info& type, bool buffers = false) {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (s.buffers == buffers && *s.type == type) {
                n += s.live_objects;
            }
        }
        return n;
    }

    // The block io::copy moved data through, as a managed object
    using ManagedCopyBlock = sgcl::array<std::byte, 32768>;

    // A reader of `size` bytes that calls `look` at its `at`-th read
    template<class F>
    struct LookingReader {
        size_t size;
        size_t at;
        F look;
        size_t done = 0;
        size_t calls = 0;

        expected<size_t, io::error> read(const slice<byte>& out) {
            if (++calls == at) {
                look();
            }
            size_t k = std::min(out.size(), size - done);
            std::memset(out.data(), 'x', k);
            done += k;
            return k;
        }

        task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }
    };

    template<class F>
    LookingReader<F> looking(size_t size, size_t at, F look) {
        return LookingReader<F>{size, at, look};
    }

    // A reader with no async_read of its own: a task's read of it runs on
    // the blocking pool. Whether each slice it was given held an owner.
    struct PoolReader {
        size_t size;
        size_t done = 0;
        std::atomic<bool> owned = true;

        expected<size_t, io::error> read(const slice<byte>& out) {
            if (!out.owner()) {
                owned = false;
            }
            size_t k = std::min(out.size(), size - done);
            std::memset(out.data(), 'x', k);
            done += k;
            return k;
        }
    };

    // A writer likewise, on the pool for a task
    struct PoolWriter {
        std::atomic<bool> owned = true;
        std::atomic<size_t> written = 0;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            if (!d.owner()) {
                owned = false;
            }
            written += d.size();
            return d.size();
        }
    };

    struct Drop {
        expected<size_t, io::error> write(const slice<const byte>& d) {
            return d.size();
        }

        task<expected<size_t, io::error>> async_write(slice<const byte> d) {
            co_return d.size();
        }
    };
}

TEST(IoHeap_Tests, CopyMakesNoManagedBlock) {
    size_t blocks = size_t(-1);
    auto r = looking(1 << 20, 2, [&] { blocks = live_of(typeid(ManagedCopyBlock)); });
    auto n = io::copy(io::discard, r);
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, size_t(1) << 20);
    EXPECT_EQ(blocks, 0u);
}

// async_copy's reads may run on the pool: the block they are given is
// managed and held by the slice
TEST(IoHeap_Tests, AsyncCopyHandsThePoolAnOwnedBlock) {
    PoolReader r{1 << 20};
    auto run = [&]() -> task<size_t> {
        Drop d;
        auto n = co_await io::async_copy(d, io::reader(r));
        co_return n ? *n : 0;
    };
    EXPECT_EQ(spawn(run()).wait(), size_t(1) << 20);
    EXPECT_TRUE(r.owned);
}

// read_all grows its scratch in plain memory and makes one vector of the
// size: the result's capacity what the pages of its size hold, and no managed buffer growing while the reads go on
// The capacity of a vector of n bytes made at once: past a page, what its
// pages hold beside the buffer's header (maker.h: whole pages), no more
inline size_t one_vector_of(size_t n) {
    size_t head = sizeof(sgcl::detail::ArrayBase);
    size_t pages = (n + head + config::page_size - 1) / config::page_size;
    return pages * config::page_size - head;
}

TEST(IoHeap_Tests, ReadAllGrowsNoManagedBuffer) {
    size_t before = live_of(typeid(std::byte[]), true);
    size_t during = size_t(-1);
    auto r = looking(100 * 1024, 4, [&] { during = live_of(typeid(std::byte[]), true); });
    auto v = io::read_all(r);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->size(), 100u * 1024);
    EXPECT_EQ(v->capacity(), one_vector_of(v->size()));
    EXPECT_LE(during, before);   // LE: a buffer of an earlier test may die meanwhile
}

TEST(IoHeap_Tests, AsyncReadAllHandsThePoolAnOwnedBlock) {
    PoolReader r{100 * 1024};
    auto run = [&]() -> task<size_t> {
        auto v = co_await io::async_read_all(io::reader(r));
        co_return v && v->capacity() == one_vector_of(v->size()) ? v->size() : 0;
    };
    EXPECT_EQ(spawn(run()).wait(), 100u * 1024);
    EXPECT_TRUE(r.owned);
}

TEST(IoHeap_Tests, AsyncReadAllGrowsNoManagedBuffer) {
    size_t before = live_of(typeid(std::byte[]), true);
    size_t during = size_t(-1);
    auto r = looking(100 * 1024, 4, [&] { during = live_of(typeid(std::byte[]), true); });
    auto run = [&]() -> task<size_t> {
        auto v = co_await io::async_read_all(r);
        co_return v && v->capacity() == one_vector_of(v->size()) ? v->size() : 0;
    };
    EXPECT_EQ(spawn(run()).wait(), 100u * 1024);
    EXPECT_LE(during, before);   // LE: a buffer of an earlier test may die meanwhile
}

// read_file of a small file: the file and the bytes, nothing more (no
// stat's name, no 8 KB for the read past the end). Measured with the
// collector paused: the managed bytes a hundred calls take from the pages
TEST(IoHeap_Tests, ReadFileMakesTheFileAndTheBytesOnly) {
    auto path = io::temp_file();
    ASSERT_TRUE(path);
    string name = path->path();
    (void)path->write(std::string("ten bytes!"));
    (void)path->close();
    size_t grown;
    {
        auto [pause, live] = collector::get_live_objects();
        auto before = collector::get_statistics().live_bytes;
        for (int i = 0; i < 100; ++i) {
            auto r = io::read_file(name);
            ASSERT_TRUE(r);
            ASSERT_EQ(r->size(), 10u);
        }
        grown = collector::get_statistics().live_bytes - before;
    }
    (void)io::remove(name);
    EXPECT_LT(grown, 256u * 1024) << "a hundred read_file of ten bytes took " << grown << " bytes of pages";   // 8 KB a call was 800 KB
}

TEST(IoHeap_Tests, BufferedWriterBlockIsNotManaged) {
    size_t before = live_of(typeid(sgcl::io::detail::IoBlock));
    sgcl::vector<io::buffered_writer> writers;
    for (int i = 0; i < 16; ++i) {
        writers.push_back(io::buffered_writer(io::discard));
        ASSERT_TRUE(writers.back().write(std::string("some bytes")));
    }
    EXPECT_EQ(live_of(typeid(sgcl::io::detail::IoBlock)), before);
    for (auto& w : writers) {
        EXPECT_EQ(w.buffered(), 10u);
        EXPECT_TRUE(w.flush());
        EXPECT_EQ(w.buffered(), 0u);
    }
}

// The first async operation moves the block into managed memory: a flush
// on the pool is given a slice that holds it
TEST(IoHeap_Tests, BufferedWriterHandsThePoolAnOwnedBlock) {
    PoolWriter out;
    auto run = [&]() -> task<bool> {
        io::buffered_writer w(out);
        bool ok = (bool)w.write(std::string("sync first, "));
        ok = ok && (bool)co_await w.async_write(std::string("then a task"));
        ok = ok && (bool)co_await w.async_flush();
        co_return ok;
    };
    EXPECT_TRUE(spawn(run()).wait());
    EXPECT_EQ(out.written.load(), 23u);
    EXPECT_TRUE(out.owned);
}
