//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress and the managed heap (the audit, 2026-09-26). zip: the reader of
// a deflated entry reads its range through a member, and the writer of one
// writes through a member: no object of its own for either while the entry
// is read or written. Every stream: a block a task's read or write is given
// may be read into or written from on the blocking pool (a stream with no
// async operation of its own, a file read at an offset), so it is managed
// and the slice holds it; a thread's calls keep plain memory. PoolReader
// and PoolWriter have no async operation of their own, so a task's
// operation on them runs on the pool, and they see the slice it was given.
#include "common.h"

#include <bzlib.h>

#include <atomic>
#include <cstring>
#include <string>

using namespace compress_test;
namespace zip = sgcl::compress::zip;
namespace tar = sgcl::compress::tar;
namespace io = sgcl::io;
using sgcl::async::spawn;
using sgcl::async::task;

namespace {
    // The live objects whose type's name holds `part`, after a full cycle
    size_t live_named(const char* part) {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (!s.buffers && std::strstr(s.type->name(), part)) {
                n += s.live_objects;
            }
        }
        return n;
    }

    // The bytes of `data`, read with no async_read of its own: a task's
    // read runs on the pool. Whether each slice it was given held an owner.
    struct PoolReader {
        std::string data;
        size_t at = 0;
        std::atomic<bool> owned = true;
        std::atomic<size_t> reads = 0;

        expected<size_t, io::error> read(const slice<byte>& out) {
            ++reads;
            if (!out.owner()) {
                owned = false;
            }
            size_t k = std::min(out.size(), data.size() - at);
            std::memcpy(out.data(), data.data() + at, k);
            at += k;
            return k;
        }
    };

    // A writer likewise, keeping what it is given
    struct PoolWriter {
        std::string data;
        std::atomic<bool> owned = true;
        std::atomic<size_t> writes = 0;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            ++writes;
            if (!d.owner()) {
                owned = false;
            }
            data.append(reinterpret_cast<const char*>(d.data()), d.size());
            return d.size();
        }
    };

    // Text of n bytes, compressible but not much
    std::string words(size_t n) {
        static const char* w[] = {"archive", "entry", "stream", "window", "block", "the", "of", "collector", "pointer", "header"};
        std::string s;
        uint32_t x = 12345;
        while (s.size() < n) {
            x = x * 1103515245u + 12345u;
            s += w[(x >> 16) % 10];
            s += std::to_string(x % 100000);   // a number after each word: a third or so of the size compressed
            s += (x >> 8) % 11 ? ' ' : '\n';
        }
        s.resize(n);
        return s;
    }

    // Bytes the test hands a writer: managed, as a caller whose data goes
    // to the pool gives them (the writers pass a caller's slice on as it is)
    sgcl::vector<std::byte> owned_bytes(const std::string& s) {
        sgcl::vector<std::byte> v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }

    // Everything a reader gives a task, read into an owned buffer
    template<class R>
    task<std::string> read_all(R& r) {
        sgcl::vector<std::byte> buf(4096);
        std::string got;
        for (;;) {
            auto n = co_await r.async_read(buf.as_slice());
            if (!n) {
                co_return "error: " + std::string(n.error().message().view());
            }
            if (*n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
        co_return got;
    }

    std::string bz_compress(const std::string& in) {
        unsigned size = unsigned(in.size() + in.size() / 100 + 600);
        std::string out(size, 0);
        EXPECT_EQ(BZ2_bzBuffToBuffCompress(out.data(), &size, const_cast<char*>(in.data()), unsigned(in.size()), 9, 0, 30), BZ_OK);
        out.resize(size);
        return out;
    }

    std::string temp_path(const char* name) {
        return std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp/") + "/" + name;
    }
}

TEST(ZipHeap_Tests, AnEntryIsReadWithoutARangeObject) {
    std::string data(10000, 'z');
    sgcl::io::buffer sink;
    zip::writer w(sink);
    ASSERT_TRUE(w.add("a.txt", bytes(data)));
    ASSERT_TRUE(w.close());
    std::string archived(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    auto a = zip::archive::from(bytes(archived));
    ASSERT_TRUE(a);
    ASSERT_EQ(a->entries()[0].method, zip::method::deflate);
    auto r = a->reader(a->entries()[0]);
    ASSERT_TRUE(r);
    std::byte first[16];
    auto n = r->read(sgcl::slice<std::byte>(first, sizeof(first)));   // the entry opened, its inflating reader made
    ASSERT_TRUE(n);
    ASSERT_GT(*n, 0u);
    EXPECT_EQ(live_named("RangeReader"), 0u);
    auto rest = r->read_all();
    ASSERT_TRUE(rest);
    EXPECT_EQ(*n + rest->size(), data.size());
}

TEST(ZipHeap_Tests, AnEntryIsWrittenWithoutASinkObject) {
    sgcl::io::buffer sink;
    zip::writer w(sink);
    auto e = w.create("a.txt");
    ASSERT_TRUE(e);
    ASSERT_TRUE(e->write(std::string(5000, 'y')));
    EXPECT_EQ(live_named("Sink"), 0u);   // the entry's writer alive, its deflating writer made
    ASSERT_TRUE(w.close());
    std::string archived(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    auto a = zip::archive::from(bytes(archived));
    ASSERT_TRUE(a);
    auto got = a->read(a->entries()[0]);
    ASSERT_TRUE(got);
    EXPECT_EQ(text(*got), std::string(5000, 'y'));
}

// The input of flate's, zlib's and gzip's readers, read into by a task on
// the pool: managed, the slice holding it
TEST(CompressHeap_Tests, InflatingReadersHandThePoolAnOwnedBlock) {
    std::string data = words(300000);
    for (int format = 0; format < 3; ++format) {
        PoolReader in;
        auto packed = format == 0 ? compress::flate::compress(bytes(data)) : format == 1 ? compress::zlib::compress(bytes(data)) : compress::gzip::compress(bytes(data));
        in.data = text(packed);
        auto got = spawn([](PoolReader& in, int format) -> task<std::string> {
            if (format == 0) {
                compress::flate::reader r{io::reader(in)};
                co_return co_await read_all(r);
            }
            if (format == 1) {
                compress::zlib::reader r{io::reader(in)};
                co_return co_await read_all(r);
            }
            compress::gzip::reader r{io::reader(in)};
            co_return co_await read_all(r);
        }(in, format)).wait();
        EXPECT_EQ(got, data) << format;
        EXPECT_GT(in.reads.load(), 3u) << format;
        EXPECT_TRUE(in.owned) << format;
    }
}

// A thread's reads keep the input in plain memory: no managed block
TEST(CompressHeap_Tests, AThreadsInflatingReaderMakesNoManagedBlock) {
    std::string data = words(100000);
    PoolReader in;
    in.data = text(compress::gzip::compress(bytes(data)));
    size_t before = live_named("ByteBlock");
    compress::gzip::reader r{io::reader(in)};
    std::byte buf[100];
    ASSERT_TRUE(r.read(sgcl::slice<std::byte>(buf, sizeof(buf))));
    EXPECT_LE(live_named("ByteBlock"), before);   // LE: a block of an earlier test may die meanwhile
    auto rest = r.read_all();
    ASSERT_TRUE(rest);
    EXPECT_EQ(rest->size(), data.size() - sizeof(buf));
}

TEST(CompressHeap_Tests, DeflatingWritersHandThePoolAnOwnedBlock) {
    std::string data = words(300000);
    auto input = owned_bytes(data);
    for (int format = 0; format < 3; ++format) {
        PoolWriter out;
        bool ok = spawn([](PoolWriter& out, int format, const sgcl::vector<std::byte>& input) -> task<bool> {
            auto run = [&](auto& w) -> task<bool> {
                // in two writes, a flush between them
                bool ok = (bool)co_await w.async_write(input.as_slice(0, 100000));
                ok = ok && (bool)co_await w.async_flush();
                ok = ok && (bool)co_await w.async_write(input.as_slice(100000));
                ok = ok && (bool)co_await w.async_close();
                co_return ok;
            };
            if (format == 0) {
                compress::flate::writer w{io::writer(out)};
                co_return co_await run(w);
            }
            if (format == 1) {
                compress::zlib::writer w{io::writer(out)};
                co_return co_await run(w);
            }
            compress::gzip::writer w{io::writer(out)};
            co_return co_await run(w);
        }(out, format, input)).wait();
        ASSERT_TRUE(ok) << format;
        auto back = format == 0 ? compress::flate::decompress(bytes(out.data)) : format == 1 ? compress::zlib::decompress(bytes(out.data)) : compress::gzip::decompress(bytes(out.data));
        ASSERT_TRUE(back) << format;
        EXPECT_EQ(text(*back), data) << format;
        EXPECT_GT(out.writes.load(), 2u) << format;
        EXPECT_TRUE(out.owned) << format;
    }
}

TEST(CompressHeap_Tests, Bzip2ReaderHandsThePoolAnOwnedBlock) {
    std::string data = words(300000);
    PoolReader in;
    in.data = bz_compress(data);
    auto got = spawn([](PoolReader& in) -> task<std::string> {
        compress::bzip2::reader r{io::reader(in)};
        co_return co_await read_all(r);
    }(in)).wait();
    EXPECT_EQ(got, data);
    EXPECT_GT(in.reads.load(), 1u);
    EXPECT_TRUE(in.owned);
}

TEST(CompressHeap_Tests, LzwReaderAndWriterHandThePoolAnOwnedBlock) {
    std::string data = words(300000);
    auto input = owned_bytes(data);
    PoolWriter out;
    bool ok = spawn([](PoolWriter& out, const sgcl::vector<std::byte>& input) -> task<bool> {
        compress::lzw::writer w(io::writer(out), compress::lzw::order::msb, 8);
        bool ok = (bool)co_await w.async_write(input.as_slice());
        co_return ok && (bool)co_await w.async_close();
    }(out, input)).wait();
    ASSERT_TRUE(ok);
    EXPECT_TRUE(out.owned);
    PoolReader in;
    in.data = out.data;
    auto got = spawn([](PoolReader& in) -> task<std::string> {
        compress::lzw::reader r(io::reader(in), compress::lzw::order::msb, 8);
        co_return co_await read_all(r);
    }(in)).wait();
    EXPECT_EQ(got, data);
    EXPECT_GT(in.reads.load(), 3u);
    EXPECT_TRUE(in.owned);
}

TEST(CompressHeap_Tests, TarReaderAndWriterHandThePoolAnOwnedBlock) {
    std::string a = words(100000), b = words(3000);
    auto da = owned_bytes(a), db = owned_bytes(b);
    std::string long_name = "dir/" + std::string(200, 'n') + ".txt";   // a pax record ahead of its header
    PoolWriter out;
    bool ok = spawn([](PoolWriter& out, const sgcl::vector<std::byte>& da, const sgcl::vector<std::byte>& db, std::string long_name) -> task<bool> {
        tar::writer w{io::writer(out)};
        tar::entry e;
        e.name = "a.txt";
        e.size = da.size();
        e.mode = io::permissions(0644);
        bool ok = (bool)co_await w.async_write_header(e);
        ok = ok && (bool)co_await w.async_write(da.as_slice());
        e.name = sgcl::string(long_name);
        e.size = db.size();
        ok = ok && (bool)co_await w.async_write_header(e);
        ok = ok && (bool)co_await w.async_write(db.as_slice());
        co_return ok && (bool)co_await w.async_close();
    }(out, da, db, long_name)).wait();
    ASSERT_TRUE(ok);
    EXPECT_TRUE(out.owned);
    PoolReader in;
    in.data = out.data;
    auto got = spawn([](PoolReader& in) -> task<std::string> {
        tar::reader r{io::reader(in)};
        std::string all;
        for (;;) {
            auto e = co_await r.async_next();
            if (!e) {
                co_return "error";
            }
            if (!*e) {
                break;
            }
            all += std::string((*e)->name.view()) + ":" + co_await read_all(r) + "|";
        }
        co_return all;
    }(in)).wait();
    EXPECT_EQ(got, "a.txt:" + a + "|" + long_name + ":" + b + "|");
    EXPECT_TRUE(in.owned);
}

// A zip written by a task: the local headers, the data (deflated, or a
// stored entry's own owned bytes), the descriptors and the directory all
// written on the pool from managed memory
TEST(ZipHeap_Tests, WriterHandsThePoolAnOwnedBlock) {
    std::string a = words(200000), b = words(5000), c = words(700);
    auto da = owned_bytes(a), db = owned_bytes(b), dc = owned_bytes(c);
    PoolWriter out;
    bool ok = spawn([](PoolWriter& out, const sgcl::vector<std::byte>& da, const sgcl::vector<std::byte>& db, const sgcl::vector<std::byte>& dc) -> task<bool> {
        zip::writer w{io::writer(out)};
        zip::entry e;
        e.name = "a.txt";
        auto ew = co_await w.async_create(e);
        if (!ew || !co_await ew->async_write(da.as_slice())) {
            co_return false;
        }
        if (!co_await w.async_add("dir/b.txt", db.as_slice())) {
            co_return false;
        }
        zip::entry s;
        s.name = "stored.txt";
        s.method = zip::method::store;
        auto sw = co_await w.async_create(s);
        if (!sw || !co_await sw->async_write(dc.as_slice())) {
            co_return false;
        }
        zip::entry d;
        d.name = "dir/";
        if (!co_await w.async_create(d)) {
            co_return false;
        }
        co_return (bool)co_await w.async_close();
    }(out, da, db, dc)).wait();
    ASSERT_TRUE(ok);
    EXPECT_TRUE(out.owned);
    auto archive = zip::archive::from(bytes(out.data));
    ASSERT_TRUE(archive);
    ASSERT_EQ(archive->entries().size(), 4u);
    EXPECT_EQ(text(value_of(archive->read("a.txt"))), a);
    EXPECT_EQ(text(value_of(archive->read("dir/b.txt"))), b);
    EXPECT_EQ(text(value_of(archive->read("stored.txt"))), c);
}

// An archive in a file: a task's reads of it run on the pool. An entry's
// reader reads the local header and the name into a block of its own, a
// deflated entry's compressed bytes into its inflater's input, both
// managed and made once for the reader; a thread's reads make neither.
TEST(ZipHeap_Tests, AFileEntryIsReadIntoManagedBlocksOfItsReader) {
    std::string a = words(100000), c = words(3000);
    std::string path = temp_path("sgcl-zip-heap.zip");
    {
        io::buffer sink;
        zip::writer w(sink);
        ASSERT_TRUE(w.add("a.txt", bytes(a)));
        zip::entry s;
        s.name = "stored.txt";
        s.method = zip::method::store;
        auto sw = w.create(s);
        ASSERT_TRUE(sw);
        ASSERT_TRUE(sw->write(bytes(c)));
        ASSERT_TRUE(w.close());
        auto f = io::create(sgcl::string(path));
        ASSERT_TRUE(f);
        ASSERT_TRUE(f->write(sink.data()));
        ASSERT_TRUE(f->close());
    }
    auto archive = zip::archive::open(sgcl::string(path));
    ASSERT_TRUE(archive);
    // the blocks by name (compress/detail/block.h), so that the test builds
    // on a tree without them
    constexpr size_t N = 4;
    // a thread's
    {
        size_t blocks = live_named("ByteBlock");
        sgcl::vector<io::reader> readers;
        for (size_t i = 0; i < N; ++i) {
            for (auto name : {"a.txt", "stored.txt"}) {
                auto r = archive->reader(sgcl::string(name));
                ASSERT_TRUE(r);
                std::byte first[16];
                ASSERT_TRUE(r->read(sgcl::slice<std::byte>(first, sizeof(first))));
                readers.push_back(*r);
            }
        }
        EXPECT_LE(live_named("ByteBlock"), blocks);
    }
    // a task's
    size_t blocks = live_named("ByteBlock");
    sgcl::vector<io::reader> readers;
    for (size_t i = 0; i < N; ++i) {
        for (auto name : {"a.txt", "stored.txt"}) {
            auto r = archive->reader(sgcl::string(name));
            ASSERT_TRUE(r);
            readers.push_back(*r);
        }
    }
    bool ok = spawn([](sgcl::vector<io::reader>& readers) -> task<bool> {
        sgcl::vector<std::byte> first(16);
        for (auto& r : readers) {
            auto n = co_await r.async_read(first.as_slice());
            if (!n || *n != 16) {
                co_return false;
            }
        }
        co_return true;
    }(readers)).wait();
    ASSERT_TRUE(ok);
    EXPECT_GE(live_named("ByteBlock"), blocks + 3 * N);   // a header's block for each reader, an input block for each of a deflated entry
    // and the rest of them, whole
    auto rest = spawn([](sgcl::vector<io::reader>& readers) -> task<std::string> {
        std::string all;
        for (size_t i = 0; i < 2; ++i) {
            all += co_await read_all(readers[i]) + "|";
        }
        co_return all;
    }(readers)).wait();
    EXPECT_EQ(rest, a.substr(16) + "|" + c.substr(16) + "|");
    readers.clear();
    (void)archive->close();
    std::remove(path.c_str());
}

// Opening an archive in a file by a task reads its tail and its directory
// on the pool, into a managed buffer made for the open: the managed bytes
// an open of an archive whose tail read is 64 KB takes (types.h:
// managed_bytes_of; caf6980 read into plain memory and took next to none)
TEST(ZipHeap_Tests, AFileArchiveIsOpenedIntoManagedMemory) {
    std::string path = temp_path("sgcl-zip-heap-open.zip");
    {
        io::buffer sink;
        zip::writer w(sink);
        zip::entry s;
        s.name = "big.bin";
        s.method = zip::method::store;
        auto sw = w.create(s);
        ASSERT_TRUE(sw);
        ASSERT_TRUE(sw->write(bytes(words(100000))));
        ASSERT_TRUE(w.close());
        auto f = io::create(sgcl::string(path));
        ASSERT_TRUE(f);
        ASSERT_TRUE(f->write(sink.data()));
        ASSERT_TRUE(f->close());
    }
    auto file = io::open(sgcl::string(path));
    ASSERT_TRUE(file);
    constexpr size_t Opens = 16;
    size_t grown = managed_bytes_of(Opens, [&] {
        bool ok = spawn([](io::file file) -> task<bool> {
            auto a = co_await zip::archive::async_open(file);
            co_return a && a->entries().size() == 1;
        }(*file)).wait();
        ASSERT_TRUE(ok);
    });
    EXPECT_GE(grown, Opens * 65536) << Opens << " opens took " << grown << " managed bytes";
    (void)file->close();
    std::remove(path.c_str());
}
