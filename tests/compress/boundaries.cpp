//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the module's readers and writers (DESIGN 408): a
// stream moved from and moved onto itself, a stream read past its end and
// with no room, a writer closed twice and written after its close, a
// source or an output that fails half-way, no data and one byte.
#include "common.h"

#include <bzlib.h>

#include <memory>

using namespace compress_test;

namespace {
    using sgcl::byte;
    using sgcl::expected;
    using sgcl::slice;
    using sgcl::unexpected;
    namespace io = sgcl::io;

    // A source of `data` that counts its closes, and fails with EIO at
    // byte `fail_at` (never, by default)
    struct source {
        std::string data;
        std::shared_ptr<int> closes = std::make_shared<int>(0);
        size_t fail_at = SIZE_MAX;
        size_t at = 0;

        expected<size_t, io::error> read(slice<byte> b) {
            if (at >= fail_at) {
                return unexpected<io::error>(io::error(sgcl::error_code(EIO, std::system_category()), "read", "disk"));
            }
            size_t n = std::min({b.size(), data.size() - at, fail_at - at});
            std::memcpy(b.data(), data.data() + at, n);
            at += n;
            return n;
        }

        expected<void, io::error> close() {
            ++*closes;
            return {};
        }
    };

    std::string contents(const io::buffer& b) {
        auto d = b.data();
        return std::string(reinterpret_cast<const char*>(d.data()), d.size());
    }

    std::string sample() {
        std::string s;
        for (int i = 0; i < 4000; ++i) {
            s += "line " + std::to_string(i * 7919 % 1000) + " of the sample\n";
        }
        return s;
    }

    std::string bz2(const std::string& in) {
        std::string out(in.size() + in.size() / 100 + 600, 0);
        unsigned size = unsigned(out.size());
        BZ2_bzBuffToBuffCompress(out.data(), &size, const_cast<char*>(in.data()), unsigned(in.size()), 9, 0, 0);
        out.resize(size);
        return out;
    }

    std::string read_rest(auto& r) {
        auto all = r.read_all();
        EXPECT_TRUE(all);
        return all ? text(*all) : std::string();
    }

    std::string unflate(const std::string& s) {
        return text(*compress::flate::decompress(bytes(s)));
    }

    std::string unzlib(const std::string& s) {
        return text(*compress::zlib::decompress(bytes(s)));
    }

    std::string gunzip(const std::string& s) {
        return text(*compress::gzip::decompress(bytes(s)));
    }

    std::string unlzma(const std::string& s) {
        return text(*compress::lzma::decompress(bytes(s)));
    }

    std::string unxz(const std::string& s) {
        return text(*compress::xz::decompress(bytes(s)));
    }

    std::string unlzw(const std::string& s) {
        return text(*compress::lzw::decompress(bytes(s), compress::lzw::order::msb, 8));
    }

    std::string unlz4(const std::string& s) {
        return text(*compress::lz4::decompress(bytes(s)));
    }

    std::string unsnappy(const std::string& s) {
        return text(*compress::snappy::decompress(bytes(s)));
    }

    std::string unzstd(const std::string& s) {
        return text(*compress::zstd::decompress(bytes(s)));
    }

    std::string unbrotli(const std::string& s) {
        return text(*compress::brotli::decompress(bytes(s)));
    }

    std::string unbzip2(const std::string& s) {
        return text(*compress::bzip2::decompress(bytes(s)));
    }

    // A writer moved from is closed: a close does nothing, a write gives
    // io::errc::closed and is kept, and a reset gives it a new stream; the
    // one moved to goes on with the stream, and a move onto itself changes
    // nothing
    template<class Make, class Decode>
    void moved_writer(Make make, Decode decode) {
        io::buffer sink;
        auto w = make(sink);
        ASSERT_TRUE(w.write(std::string("first, ")));
        auto moved = std::move(w);
        EXPECT_TRUE(w.is_closed());
        EXPECT_FALSE(w.last_error());
        EXPECT_TRUE(w.close());   // nothing to end: it has no stream
        auto late = w.write(std::string("lost"));
        ASSERT_FALSE(late);
        EXPECT_TRUE(late.error().is_closed());
        ASSERT_TRUE(w.last_error());
        EXPECT_TRUE(*w.last_error() == late.error());
        auto closed = w.close();
        ASSERT_FALSE(closed);
        EXPECT_TRUE(closed.error() == late.error());

        auto& same = moved;
        moved = std::move(same);   // onto itself: nothing changes
        EXPECT_FALSE(moved.is_closed());
        ASSERT_TRUE(moved.write(std::string("second")));
        ASSERT_TRUE(moved.close());
        EXPECT_EQ(decode(contents(sink)), "first, second");

        // the one moved from given a new stream
        io::buffer again;
        w.reset(again);
        EXPECT_FALSE(w.is_closed());
        EXPECT_FALSE(w.last_error());
        ASSERT_TRUE(w.write(std::string("third")));
        ASSERT_TRUE(w.close());
        EXPECT_EQ(decode(contents(again)), "third");

        // moved onto a writer that was writing: its stream dropped, the other's taken
        io::buffer a, b;
        auto x = make(a);
        auto y = make(b);
        ASSERT_TRUE(x.write(std::string("x's")));
        ASSERT_TRUE(y.write(std::string("y's")));
        x = std::move(y);
        EXPECT_TRUE(y.is_closed());
        ASSERT_TRUE(x.close());
        EXPECT_EQ(decode(contents(b)), "y's");
    }

    // A reader moved from has no stream: its reads give io::errc::closed
    // (its last_error says so), its close closes nothing, and a reset gives
    // it a new stream; the one moved to goes on where it was
    template<class Make>
    void moved_reader(const std::string& packed, const std::string& plain, Make make) {
        source src{packed};
        auto closes = src.closes;
        auto r = make(io::reader(src));
        std::string head(100, 0);
        ASSERT_TRUE(r.read_full(slice<byte>(reinterpret_cast<byte*>(head.data()), head.size())));
        auto moved = std::move(r);
        byte one[1];
        auto late = r.read(one);
        ASSERT_FALSE(late);
        EXPECT_TRUE(late.error().is_closed());
        ASSERT_TRUE(r.last_error());
        EXPECT_EQ(r.last_error()->code(), compress::errc::io);
        EXPECT_EQ(r.last_error()->offset(), 0u);
        EXPECT_TRUE(r.close());
        EXPECT_EQ(*closes, 0);   // the stream is the other's

        auto& same = moved;
        moved = std::move(same);   // onto itself: nothing changes
        EXPECT_EQ(head + read_rest(moved), plain);
        EXPECT_TRUE(moved.close());
        EXPECT_EQ(*closes, 1);

        r.reset(io::reader(source{packed}));
        EXPECT_FALSE(r.last_error());
        EXPECT_EQ(read_rest(r), plain);
    }

    // Past its end a reader gives 0, every time; a read with no room gives
    // 0 before the end and after it
    template<class Make>
    void past_the_end(const std::string& packed, const std::string& plain, Make make) {
        auto r = make(io::reader(source{packed}));
        EXPECT_EQ(r.read(slice<byte>()).value_or(99), 0u);
        EXPECT_EQ(read_rest(r), plain);
        byte b[16];
        for (int i = 0; i < 3; ++i) {
            auto n = r.read(b);
            ASSERT_TRUE(n);
            EXPECT_EQ(*n, 0u);
        }
        EXPECT_EQ(r.read(slice<byte>()).value_or(99), 0u);
        EXPECT_FALSE(r.last_error());
    }

    // A source that fails half-way: the error of the source, kept with the
    // offset reached, and the same for every read after
    template<class Make>
    void failing_source(const std::string& packed, Make make) {
        source src{packed};
        src.fail_at = packed.size() / 2;
        auto r = make(io::reader(src));
        auto all = r.read_all();
        ASSERT_FALSE(all);
        EXPECT_TRUE(is_eio(all.error()));
        ASSERT_TRUE(r.last_error());
        EXPECT_EQ(r.last_error()->code(), compress::errc::io);
        ASSERT_TRUE(r.last_error()->io_error());
        EXPECT_TRUE(is_eio(*r.last_error()->io_error()));
        EXPECT_LE(r.last_error()->offset(), packed.size() / 2);
        byte b[16];
        auto again = r.read(b);
        ASSERT_FALSE(again);
        EXPECT_TRUE(is_eio(again.error()));
    }

    // Data cut to its first byte, and no data at all: unexpected_end
    template<class Make>
    void cut_short(const std::string& packed, Make make) {
        for (size_t n : {size_t(0), size_t(1)}) {
            SCOPED_TRACE(n);
            auto r = make(io::reader(source{packed.substr(0, n)}));
            auto all = r.read_all();
            ASSERT_FALSE(all);
            ASSERT_TRUE(r.last_error());
            EXPECT_EQ(r.last_error()->code(), compress::errc::unexpected_end);
        }
    }

    auto flate_reader = [](const io::reader& in) { return compress::flate::reader(in); };
    auto zlib_reader = [](const io::reader& in) { return compress::zlib::reader(in); };
    auto gzip_reader = [](const io::reader& in) { return compress::gzip::reader(in); };
    auto lzma_reader = [](const io::reader& in) { return compress::lzma::reader(in); };
    auto xz_reader = [](const io::reader& in) { return compress::xz::reader(in); };
    auto lzw_reader = [](const io::reader& in) { return compress::lzw::reader(in, compress::lzw::order::msb, 8); };
    auto bzip2_reader = [](const io::reader& in) { return compress::bzip2::reader(in); };
    auto lz4_reader = [](const io::reader& in) { return compress::lz4::reader(in); };
    auto snappy_reader = [](const io::reader& in) { return compress::snappy::reader(in); };
    auto zstd_reader = [](const io::reader& in) { return compress::zstd::reader(in); };
    auto brotli_reader = [](const io::reader& in) { return compress::brotli::reader(in); };

    // Each format's bytes of `plain`, with the reader that reads them
    template<class F>
    void each_format(const std::string& plain, F f) {
        {
            SCOPED_TRACE("flate");
            f(text(compress::flate::compress(bytes(plain))), flate_reader);
        }
        {
            SCOPED_TRACE("zlib");
            f(text(compress::zlib::compress(bytes(plain))), zlib_reader);
        }
        {
            SCOPED_TRACE("gzip");
            f(text(compress::gzip::compress(bytes(plain))), gzip_reader);
        }
        {
            SCOPED_TRACE("lzma");
            f(text(compress::lzma::compress(bytes(plain))), lzma_reader);
        }
        {
            SCOPED_TRACE("xz");
            f(text(compress::xz::compress(bytes(plain))), xz_reader);
        }
        {
            SCOPED_TRACE("lzw");
            f(text(compress::lzw::compress(bytes(plain), compress::lzw::order::msb, 8)), lzw_reader);
        }
        {
            SCOPED_TRACE("bzip2");
            f(bz2(plain), bzip2_reader);
        }
        {
            SCOPED_TRACE("lz4");
            f(text(compress::lz4::compress(bytes(plain), {.block_size = compress::lz4::block_size::kb64, .linked_blocks = true})), lz4_reader);
        }
        {
            SCOPED_TRACE("snappy");
            f(text(compress::snappy::compress(bytes(plain))), snappy_reader);
        }
        {
            SCOPED_TRACE("zstd");
            f(text(compress::zstd::compress(bytes(plain))), zstd_reader);
        }
        {
            SCOPED_TRACE("brotli");
            f(text(compress::brotli::compress(bytes(plain), {.level = 5})), brotli_reader);
        }
    }
}

TEST(CompressBoundaries_Tests, MovedWriters) {
    const std::string dict = "first, second, third";
    moved_writer([](io::buffer& s) { return compress::flate::writer(s); }, unflate);
    moved_writer([&](io::buffer& s) { return compress::zlib::writer(s, {.dictionary = bytes(dict)}); },
                 [&](const std::string& s) { return text(*compress::zlib::decompress(bytes(s), {.dictionary = bytes(dict)})); });
    moved_writer([](io::buffer& s) { return compress::gzip::writer(s, {.header = {.name = "a.txt"}}); }, gunzip);
    moved_writer([](io::buffer& s) { return compress::lzma::writer(s, {.level = 1}); }, unlzma);
    moved_writer([](io::buffer& s) { return compress::xz::writer(s, {.level = 1, .delta = 2}); }, unxz);
    moved_writer([](io::buffer& s) { return compress::lzw::writer(s, compress::lzw::order::msb, 8); }, unlzw);
    moved_writer([](io::buffer& s) { return compress::lz4::writer(s, {.level = 9}); }, unlz4);
    moved_writer([](io::buffer& s) { return compress::snappy::writer(s); }, unsnappy);
    moved_writer([](io::buffer& s) { return compress::zstd::writer(s, {.level = 9}); }, unzstd);
    moved_writer([](io::buffer& s) { return compress::brotli::writer(s, {.level = 5}); }, unbrotli);
    moved_writer([](io::buffer& s) { return compress::bzip2::writer(s, {.level = 1}); }, unbzip2);
}

TEST(CompressBoundaries_Tests, MovedReaders) {
    const std::string plain = sample();
    each_format(plain, [&](const std::string& packed, auto make) { moved_reader(packed, plain, make); });
    const std::string dict = "line of the sample";
    moved_reader(text(compress::zlib::compress(bytes(plain), {.dictionary = bytes(dict)})), plain,
                 [&](const io::reader& in) { return compress::zlib::reader(in, {.dictionary = bytes(dict)}); });
}

// The header of a gzip reader moved from: none, the stream is the other's
TEST(CompressBoundaries_Tests, AMovedGzipReadersHeader) {
    auto packed = text(compress::gzip::compress(bytes(std::string("data")), {.header = {.name = "n.txt"}}));
    compress::gzip::reader r(io::reader(source{packed}));
    auto moved = std::move(r);
    auto h = r.header();
    ASSERT_FALSE(h);
    EXPECT_EQ(h.error().code(), compress::errc::io);
    auto mh = moved.header();
    ASSERT_TRUE(mh);
    EXPECT_EQ(mh->name, "n.txt");
}

TEST(CompressBoundaries_Tests, ReadsPastTheEnd) {
    for (const std::string& plain : {std::string(), std::string("x"), sample()}) {
        each_format(plain, [&](const std::string& packed, auto make) { past_the_end(packed, plain, make); });
    }
}

TEST(CompressBoundaries_Tests, ASourceThatFailsHalfWay) {
    each_format(sample(), [](const std::string& packed, auto make) { failing_source(packed, make); });
}

TEST(CompressBoundaries_Tests, NoDataAndOneByte) {
    each_format("some text", [](const std::string& packed, auto make) { cut_short(packed, make); });
    // in memory the same
    const std::string none;
    EXPECT_EQ(compress::flate::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::zlib::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::gzip::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::lzma::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::xz::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::bzip2::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::lzw::decompress(bytes(none), compress::lzw::order::lsb, 8).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::lz4::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::snappy::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::snappy::decompress_block(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::snappy::decompress(bytes(std::string("\xff", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::lz4::decompress(bytes(std::string("\x04", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::zstd::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::zstd::decompress(bytes(std::string("\x28", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::brotli::decompress(bytes(none)).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::brotli::decompress(bytes(std::string("\x01", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_FALSE(compress::zstd::content_size(bytes(none)));
    EXPECT_EQ(compress::zstd::dictionary_id(bytes(none)), 0u);
    // one byte: a final fixed block cut before its end code; two make the empty stream
    EXPECT_EQ(compress::flate::decompress(bytes(std::string("\x03", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::flate::decompress(bytes(std::string("\x03\x00", 2)))->size(), 0u);
    EXPECT_EQ(compress::bzip2::decompress(bytes(std::string("B"))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::zlib::decompress(bytes(std::string("\x78", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::gzip::decompress(bytes(std::string("\x1f", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::xz::decompress(bytes(std::string("\xfd", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_EQ(compress::lzma::decompress(bytes(std::string("\x5d", 1))).error().code(), compress::errc::unexpected_end);
    EXPECT_FALSE(compress::zlib::dictionary_id(bytes(none)));
    EXPECT_FALSE(compress::zlib::dictionary_id(bytes(std::string("\x78", 1))));
}

// A write after the close is kept as the first error, by every writer of
// one stream; a second close after a close that succeeded does nothing
TEST(CompressBoundaries_Tests, AWriteAfterTheCloseIsKept) {
    auto check = [](auto&& w, io::buffer& sink) {
        ASSERT_TRUE(w.write(std::string("data")));
        ASSERT_TRUE(w.close());
        size_t size = sink.size();
        ASSERT_TRUE(w.close());
        EXPECT_EQ(sink.size(), size);
        auto late = w.write(std::string("x"));
        ASSERT_FALSE(late);
        EXPECT_TRUE(late.error().is_closed());
        ASSERT_TRUE(w.last_error());
        EXPECT_TRUE(*w.last_error() == late.error());
        auto closed = w.close();
        ASSERT_FALSE(closed);
        EXPECT_TRUE(closed.error() == late.error());
        EXPECT_EQ(sink.size(), size);
    };
    io::buffer a, b, c, d;
    check(compress::lzma::writer(a), a);
    check(compress::xz::writer(b), b);
    check(compress::lzw::writer(c, compress::lzw::order::lsb, 8), c);
    check(compress::lz4::writer(d), d);
    io::buffer e;
    check(compress::snappy::writer(e), e);
    io::buffer f;
    check(compress::zstd::writer(f), f);
    io::buffer g;
    check(compress::brotli::writer(g, {.level = 1}), g);
    io::buffer h;
    check(compress::bzip2::writer(h, {.level = 1}), h);
}

// Nothing written: each writer's empty stream, which reads as no bytes; a
// write of no bytes writes nothing either
TEST(CompressBoundaries_Tests, EmptyStreams) {
    auto check = [](auto&& w, io::buffer& sink, auto decode) {
        ASSERT_TRUE(w.write(slice<const byte>()));
        ASSERT_TRUE(w.close());
        EXPECT_GT(sink.size(), 0u);
        EXPECT_EQ(decode(contents(sink)), "");
    };
    io::buffer a, b, c, d, e, f, g;
    check(compress::lz4::writer(g), g, unlz4);
    io::buffer h;
    check(compress::snappy::writer(h), h, unsnappy);
    io::buffer i;
    check(compress::zstd::writer(i), i, unzstd);
    io::buffer j;
    check(compress::brotli::writer(j), j, unbrotli);
    io::buffer k;
    check(compress::bzip2::writer(k), k, unbzip2);
    check(compress::flate::writer(a), a, unflate);
    check(compress::zlib::writer(b), b, unzlib);
    check(compress::gzip::writer(c), c, gunzip);
    check(compress::lzma::writer(d), d, unlzma);
    check(compress::xz::writer(e), e, unxz);
    check(compress::lzw::writer(f, compress::lzw::order::msb, 8), f, unlzw);
}

// The writers into an output that fails half-way: the failure kept, the
// next write and close give it, nothing more goes out
TEST(CompressBoundaries_Tests, AnOutputThatFailsHalfWay) {
    std::mt19937 rng(3);
    std::string plain(300000, 0);
    for (auto& ch : plain) {
        ch = char(rng());
    }
    auto check = [&](auto&& w, failing_after& out) {
        for (size_t i = 0; i < plain.size(); i += 50000) {
            if (!w.write(bytes(plain.substr(i, 50000)))) {
                break;
            }
        }
        auto c = w.close();
        ASSERT_FALSE(c);
        EXPECT_TRUE(is_eio(c.error()));
        ASSERT_TRUE(w.last_error());
        EXPECT_TRUE(is_eio(*w.last_error()));
        int calls = out.calls;
        EXPECT_FALSE(w.write(std::string("more")));
        EXPECT_FALSE(w.close());
        EXPECT_EQ(out.calls, calls);
        EXPECT_EQ(out.failures, 1);
        EXPECT_LE(out.taken, 1000u);
    };
    failing_after a(1000), b(1000), c(1000), d(1000), e(1000), f(1000), g(1000);
    check(compress::lz4::writer(g, {.block_size = compress::lz4::block_size::kb64}), g);
    failing_after h(1000);
    check(compress::snappy::writer(h), h);
    failing_after i(1000);
    check(compress::zstd::writer(i, {.level = 1}), i);
    failing_after j(1000);
    check(compress::brotli::writer(j, {.level = 1}), j);
    failing_after k(1000);
    check(compress::bzip2::writer(k, {.level = 1}), k);
    check(compress::flate::writer(a), a);
    check(compress::zlib::writer(b), b);
    check(compress::gzip::writer(c), c);
    check(compress::lzma::writer(d, {.level = 0}), d);
    check(compress::xz::writer(e, {.level = 0}), e);
    check(compress::lzw::writer(f, compress::lzw::order::msb, 8), f);
}

namespace {
    namespace tar = compress::tar;
    namespace zip = compress::zip;
    namespace sevenzip = compress::sevenzip;

    tar::entry tar_file(const std::string& name, uint64_t size) {
        tar::entry e;
        e.name = sgcl::string(name);
        e.size = size;
        e.mode = io::permissions(0644);
        e.modified = sgcl::time::datetime::from_unix(1700000000, sgcl::time::zone::utc());
        return e;
    }

    // A zip archive of stored entries made by hand, each with the size
    // its central record declares (in a ZIP64 field when past 32 bits)
    struct raw_entry {
        std::string name;
        std::string data;
        uint64_t declared;
    };

    void le(std::string& o, uint64_t v, int n) {
        for (int i = 0; i < n; ++i) {
            o += char(uint8_t(v >> (8 * i)));
        }
    }

    std::string raw_zip(const std::vector<raw_entry>& entries) {
        std::string out, central;
        for (auto& e : entries) {
            uint32_t crc = uint32_t(crc32(0, reinterpret_cast<const Bytef*>(e.data.data()), uInt(e.data.size())));
            bool big = e.declared >= 0xFFFFFFFF;
            uint64_t offset = out.size();
            le(out, 0x04034b50, 4);
            le(out, 20, 2);
            le(out, 0, 2);
            le(out, 0, 2);
            le(out, 0, 4);
            le(out, crc, 4);
            le(out, e.data.size(), 4);
            le(out, e.data.size(), 4);
            le(out, e.name.size(), 2);
            le(out, 0, 2);
            out += e.name + e.data;
            le(central, 0x02014b50, 4);
            le(central, 45, 2);
            le(central, 45, 2);
            le(central, 0, 2);
            le(central, 0, 2);
            le(central, 0, 4);
            le(central, crc, 4);
            le(central, e.data.size(), 4);
            le(central, big ? 0xFFFFFFFF : e.declared, 4);
            le(central, e.name.size(), 2);
            le(central, big ? 12 : 0, 2);
            le(central, 0, 2);
            le(central, 0, 2);
            le(central, 0, 2);
            le(central, 0, 4);
            le(central, offset, 4);
            central += e.name;
            if (big) {
                le(central, 1, 2);
                le(central, 8, 2);
                le(central, e.declared, 8);
            }
        }
        uint64_t at = out.size();
        out += central;
        le(out, 0x06054b50, 4);
        le(out, 0, 4);
        le(out, entries.size(), 2);
        le(out, entries.size(), 2);
        le(out, central.size(), 4);
        le(out, at, 4);
        le(out, 0, 2);
        return out;
    }

    void put_file(const std::filesystem::path& p, const std::string& s) {
        std::ofstream os(p, std::ios::binary);
        os.write(s.data(), std::streamsize(s.size()));
    }

    bool empty_dir(const std::filesystem::path& p) {
        return !std::filesystem::exists(p) || std::filesystem::is_empty(p);
    }

    sgcl::string path_of(const std::filesystem::path& p) {
        return sgcl::string(p.string());
    }
}

// The bound of extract on the entries' sizes together: two sizes whose sum
// passes 2^64 are not taken for a small total, and nothing is written
TEST(CompressBoundaries_Tests, ExtractsBoundIsNotWrappedPast64Bits) {
    scratch_dir scratch("sgcl-bound");
    auto archive = scratch.path() / "wrap.zip";
    put_file(archive, raw_zip({{"a.txt", std::string(100, 'a'), 100}, {"b.txt", "b", UINT64_MAX - 49}}));
    auto out = scratch.path() / "out";
    auto r = zip::extract(path_of(archive), path_of(out), {.max_size = 1000});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    EXPECT_TRUE(empty_dir(out));
    // exactly at the bound, and one past it
    auto fits = scratch.path() / "fits.zip";
    put_file(fits, raw_zip({{"a.txt", std::string(600, 'a'), 600}, {"b.txt", std::string(400, 'b'), 400}}));
    EXPECT_TRUE(zip::extract(path_of(fits), path_of(scratch.path() / "at"), {.max_size = 1000}));
    EXPECT_EQ(slurp(scratch.path() / "at" / "b.txt"), std::string(400, 'b'));
    auto past = zip::extract(path_of(fits), path_of(scratch.path() / "past"), {.max_size = 999});
    ASSERT_FALSE(past);
    EXPECT_EQ(past.error().code(), compress::errc::too_large);
    EXPECT_TRUE(empty_dir(scratch.path() / "past"));
}

// Archives with no entries: written, read and extracted
TEST(CompressBoundaries_Tests, ArchivesWithNoEntries) {
    io::buffer z;
    zip::writer zw(z);
    ASSERT_TRUE(zw.close());
    EXPECT_EQ(z.size(), 22u);
    auto za = zip::archive::from(z.data());
    ASSERT_TRUE(za);
    EXPECT_TRUE(za->entries().empty());
    EXPECT_FALSE(za->find("a"));
    EXPECT_EQ(za->read("a").error().code(), compress::errc::invalid_argument);

    io::buffer t;
    tar::writer tw(t);
    ASSERT_TRUE(tw.close());
    EXPECT_EQ(contents(t), std::string(1024, '\0'));
    tar::reader tr(io::reader(source{contents(t)}));
    byte none[4];
    EXPECT_EQ(tr.read(none).value_or(99), 0u);   // before the first entry
    for (int i = 0; i < 2; ++i) {
        auto n = tr.next();
        ASSERT_TRUE(n);
        EXPECT_FALSE(*n);
    }

    io::buffer s;
    sevenzip::writer sw(s);
    ASSERT_TRUE(sw.close());
    auto sa = sevenzip::archive::from(s.data());
    ASSERT_TRUE(sa);
    EXPECT_TRUE(sa->entries().empty());
    int walked = 0;
    for (auto& [e, r] : sa->walk()) {
        (void)e;
        (void)r;
        ++walked;
    }
    EXPECT_EQ(walked, 0);

    scratch_dir scratch("sgcl-none");
    put_file(scratch.path() / "none.zip", contents(z));
    put_file(scratch.path() / "none.tar", contents(t));
    put_file(scratch.path() / "none.7z", contents(s));
    EXPECT_TRUE(zip::extract(path_of(scratch.path() / "none.zip"), path_of(scratch.path() / "z")));
    EXPECT_TRUE(tar::extract(path_of(scratch.path() / "none.tar"), path_of(scratch.path() / "t")));
    EXPECT_TRUE(sevenzip::extract(path_of(scratch.path() / "none.7z"), path_of(scratch.path() / "s")));
    for (auto d : {"z", "t", "s"}) {
        EXPECT_TRUE(std::filesystem::is_directory(scratch.path() / d)) << d;
        EXPECT_TRUE(empty_dir(scratch.path() / d)) << d;
    }
    // a file of no bytes is no zip and no 7z; a tar of no bytes ends at once
    put_file(scratch.path() / "empty", "");
    auto ez = zip::archive::open(path_of(scratch.path() / "empty"));
    ASSERT_FALSE(ez);
    EXPECT_EQ(ez.error().code(), compress::errc::invalid_header);
    auto es = sevenzip::archive::open(path_of(scratch.path() / "empty"));
    ASSERT_FALSE(es);
    EXPECT_EQ(es.error().code(), compress::errc::unexpected_end);
    EXPECT_TRUE(tar::extract(path_of(scratch.path() / "empty"), path_of(scratch.path() / "e")));
    // one byte short of the end record
    EXPECT_EQ(zip::archive::from(bytes(contents(z).substr(1))).error().code(), compress::errc::invalid_header);
}

// zip's names and comments: 65535 bytes is the format's most, 65536 refused
TEST(CompressBoundaries_Tests, ZipNamesAndCommentsAtTheLimit) {
    const std::string name(65535, 'n');
    io::buffer out;
    zip::writer w(out);
    ASSERT_TRUE(w.add(sgcl::string(name), bytes(std::string("data"))));
    ASSERT_TRUE(w.set_comment(sgcl::string(std::string(65535, 'c'))));
    ASSERT_TRUE(w.close());
    auto a = zip::archive::from(out.data());
    ASSERT_TRUE(a);
    ASSERT_EQ(a->entries().size(), 1u);
    EXPECT_EQ(a->entries()[0].name.size(), 65535u);
    EXPECT_EQ(a->comment().size(), 65535u);
    EXPECT_EQ(text(*a->read(sgcl::string(name))), "data");
    for (int which = 0; which < 3; ++which) {
        SCOPED_TRACE(which);
        io::buffer o;
        zip::writer z(o);
        zip::entry e;
        e.name = which == 0 ? sgcl::string(std::string(65536, 'n')) : sgcl::string("a");
        if (which == 1) {
            e.comment = sgcl::string(std::string(65536, 'c'));
        }
        compress::error got;
        if (which == 2) {
            auto r = z.set_comment(sgcl::string(std::string(65536, 'c')));
            ASSERT_FALSE(r);
            got = r.error();
        } else {
            auto r = z.create(e);
            ASSERT_FALSE(r);
            got = r.error();
        }
        EXPECT_EQ(got.code(), compress::errc::invalid_argument);
        EXPECT_EQ(got.offset(), 0u);
        auto c = z.close();
        ASSERT_FALSE(c);
        EXPECT_TRUE(c.error() == got);
    }
}

// The zip writer closed twice, and called after its close: set_comment's
// comment would be lost, so it is refused as create is
TEST(CompressBoundaries_Tests, ZipWriterAfterItsClose) {
    io::buffer out;
    zip::writer w(out);
    ASSERT_TRUE(w.add("a.txt", bytes(std::string("data"))));
    ASSERT_TRUE(w.close());
    size_t size = out.size();
    ASSERT_TRUE(w.close());
    EXPECT_EQ(out.size(), size);
    EXPECT_TRUE(w.is_closed());
    auto c = w.set_comment("late");
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), compress::errc::invalid_argument);
    EXPECT_EQ(std::string(c.error().message().view()), "zip: set_comment after close");
    ASSERT_TRUE(w.last_error());
    EXPECT_TRUE(*w.last_error() == c.error());
    EXPECT_FALSE(w.close());
    auto late = w.create("b.txt");   // an error kept: a writer whose writes give it
    ASSERT_TRUE(late);
    EXPECT_FALSE(late->write(std::string("x")));
    EXPECT_EQ(out.size(), size);
    EXPECT_TRUE(zip::archive::from(out.data()));
}

// A zip or 7z writer moved from is closed, with no archive: what it is
// asked is refused as after a close, and the one moved to writes on
TEST(CompressBoundaries_Tests, MovedArchiveWriters) {
    {
        io::buffer out;
        zip::writer w(out);
        ASSERT_TRUE(w.add("a.txt", bytes(std::string("first"))));
        auto moved = std::move(w);
        EXPECT_TRUE(w.is_closed());
        EXPECT_FALSE(w.last_error());
        EXPECT_TRUE(w.close());
        size_t size = out.size();
        auto late = w.create("lost.txt");
        ASSERT_FALSE(late);
        EXPECT_EQ(late.error().code(), compress::errc::invalid_argument);
        EXPECT_FALSE(w.add("lost.txt", bytes(std::string("x"))));
        EXPECT_FALSE(w.close());
        EXPECT_EQ(out.size(), size);
        auto& same = moved;
        moved = std::move(same);
        ASSERT_TRUE(moved.add("b.txt", bytes(std::string("second"))));
        ASSERT_TRUE(moved.close());
        auto a = zip::archive::from(out.data());
        ASSERT_TRUE(a);
        ASSERT_EQ(a->entries().size(), 2u);
        EXPECT_EQ(text(*a->read("b.txt")), "second");
    }
    {
        io::buffer out;
        sevenzip::writer w(out);
        w.add("a.txt", bytes(std::string("first")));
        auto moved = std::move(w);
        EXPECT_TRUE(w.is_closed());
        EXPECT_FALSE(w.last_error());
        EXPECT_TRUE(w.close());
        auto lost = w.create("lost.txt");
        EXPECT_FALSE(lost.write(std::string("x")));
        w.add_directory("d");
        ASSERT_TRUE(w.last_error());
        EXPECT_FALSE(w.close());
        auto& same = moved;
        moved = std::move(same);
        moved.add("b.txt", bytes(std::string("second")));
        ASSERT_TRUE(moved.close());
        auto a = sevenzip::archive::from(out.data());
        ASSERT_TRUE(a);
        ASSERT_EQ(a->entries().size(), 2u);
        EXPECT_EQ(text(*a->read("b.txt")), "second");
    }
}

// A 7z writer closed twice: the second does nothing; anything after the
// close is refused and kept
TEST(CompressBoundaries_Tests, SevenZipWriterAfterItsClose) {
    io::buffer out;
    sevenzip::writer w(out);
    w.add("a.txt", bytes(std::string("data")));
    ASSERT_TRUE(w.close());
    size_t size = out.size();
    ASSERT_TRUE(w.close());
    EXPECT_EQ(out.size(), size);
    w.add("b.txt", bytes(std::string("late")));
    ASSERT_TRUE(w.last_error());
    EXPECT_FALSE(w.close());
    EXPECT_EQ(out.size(), size);
    EXPECT_TRUE(sevenzip::archive::from(out.data()));
}

// 7z's count of entries against max_entries: exactly at it, and one past
TEST(CompressBoundaries_Tests, SevenZipEntriesAtTheLimit) {
    io::buffer out;
    sevenzip::writer w(out);
    for (auto n : {"a", "b", "c"}) {
        w.add(n, bytes(std::string(n)));
    }
    ASSERT_TRUE(w.close());
    EXPECT_TRUE(sevenzip::archive::from(out.data(), compress::limits{.max_entries = 3}));
    auto past = sevenzip::archive::from(out.data(), compress::limits{.max_entries = 2});
    ASSERT_FALSE(past);
    EXPECT_EQ(past.error().code(), compress::errc::too_large);
}

// An entry read whole against the limit: its size exactly at max_size, and one past
TEST(CompressBoundaries_Tests, ArchiveEntriesAtTheSizeLimit) {
    const std::string data(1000, 'q');
    io::buffer z;
    zip::writer zw(z);
    ASSERT_TRUE(zw.add("q", bytes(data)));
    ASSERT_TRUE(zw.close());
    auto za = zip::archive::from(z.data());
    auto ze = za->find("q");
    EXPECT_EQ(text(*za->read(*ze, {.max_size = 1000})), data);
    EXPECT_EQ(za->read(*ze, {.max_size = 999}).error().code(), compress::errc::too_large);
    io::buffer s;
    sevenzip::writer sw(s);
    sw.add("q", bytes(data));
    ASSERT_TRUE(sw.close());
    auto sa = sevenzip::archive::from(s.data());
    auto se = sa->find("q");
    EXPECT_EQ(text(*sa->read(*se, {.max_size = 1000})), data);
    EXPECT_EQ(sa->read(*se, {.max_size = 999}).error().code(), compress::errc::too_large);
    // an entry's reader past its end: 0, every time
    auto r = za->reader(*ze);
    EXPECT_EQ(read_rest(*r), data);
    byte b[8];
    EXPECT_EQ(r->read(b).value_or(99), 0u);
    EXPECT_EQ(r->read(b).value_or(99), 0u);
}

// tar's names at ustar's limits: 100 bytes in the name field, 155 and 100
// split at a slash, one more in a pax record; read back as written
TEST(CompressBoundaries_Tests, TarNamesAtUstarsLimits) {
    auto pax = [](const tar::entry& e) {
        io::buffer out;
        tar::writer w(out);
        EXPECT_TRUE(w.write_header(e));
        return out.size() > 512;
    };
    const std::string prefix(155, 'p'), base(100, 'n');
    EXPECT_FALSE(pax(tar_file(base, 0)));
    EXPECT_TRUE(pax(tar_file(base + "n", 0)));
    EXPECT_FALSE(pax(tar_file(prefix + "/" + base, 0)));
    EXPECT_TRUE(pax(tar_file(prefix + "p/" + base, 0)));
    EXPECT_TRUE(pax(tar_file(prefix + "/" + base + "n", 0)));
    // ustar's size field holds 2^33 - 1; 2^33 takes a pax record
    EXPECT_FALSE(pax(tar_file("big", (uint64_t(1) << 33) - 1)));
    EXPECT_TRUE(pax(tar_file("big", uint64_t(1) << 33)));
    for (auto name : {base, prefix + "/" + base, prefix + "p/" + base}) {
        io::buffer out;
        tar::writer w(out);
        ASSERT_TRUE(w.write_header(tar_file(name, 3)));
        ASSERT_TRUE(w.write(std::string("abc")));
        ASSERT_TRUE(w.close());
        tar::reader r(io::reader(source{contents(out)}));
        auto e = r.next();
        ASSERT_TRUE(e && *e);
        EXPECT_EQ(std::string((*e)->name.view()), name);
        EXPECT_EQ(read_rest(r), "abc");
    }
}

// The tar writer: closed twice, written after its close, and into an
// output that fails in the middle of an entry
TEST(CompressBoundaries_Tests, TarWriterAtItsEnds) {
    io::buffer out;
    tar::writer w(out);
    ASSERT_TRUE(w.write_header(tar_file("a", 2)));
    ASSERT_TRUE(w.write(std::string("ab")));
    ASSERT_TRUE(w.close());
    size_t size = out.size();
    ASSERT_TRUE(w.close());
    EXPECT_EQ(out.size(), size);
    auto late = w.write(std::string("x"));
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_closed());
    ASSERT_TRUE(w.last_error());
    EXPECT_FALSE(w.write_header(tar_file("b", 0)));
    EXPECT_FALSE(w.close());
    EXPECT_EQ(out.size(), size);

    failing_after sink(1000);
    tar::writer f(sink);
    ASSERT_TRUE(f.write_header(tar_file("big", 4000)));
    auto first = f.write(bytes(std::string(2000, 'x')));
    ASSERT_FALSE(first);
    EXPECT_TRUE(is_eio(first.error()));
    int calls = sink.calls;
    EXPECT_FALSE(f.write(bytes(std::string(2000, 'x'))));
    auto c = f.close();
    ASSERT_FALSE(c);
    EXPECT_TRUE(is_eio(c.error()));
    ASSERT_TRUE(f.last_error());
    EXPECT_EQ(f.last_error()->code(), compress::errc::io);
    EXPECT_EQ(sink.calls, calls);
}

// A tar write before any header: refused, kept, and said as it is (no
// entry to name); one past the entry's size names the entry
TEST(CompressBoundaries_Tests, TarWritesOutsideAnEntry) {
    io::buffer out;
    tar::writer w(out);
    auto early = w.write(std::string("x"));
    ASSERT_FALSE(early);
    ASSERT_TRUE(w.last_error());
    EXPECT_EQ(std::string(w.last_error()->message().view()), "tar: a write before any header");
    EXPECT_EQ(w.last_error()->code(), compress::errc::invalid_argument);
    EXPECT_EQ(out.size(), 0u);
    EXPECT_FALSE(w.write(slice<const byte>()));   // even no bytes: the error kept is every write's
    io::buffer out2;
    tar::writer v(out2);
    ASSERT_TRUE(v.write(slice<const byte>()));   // no bytes before a header: nothing to refuse
    ASSERT_TRUE(v.write_header(tar_file("a.txt", 2)));
    ASSERT_FALSE(v.write(std::string("abc")));
    EXPECT_EQ(std::string(v.last_error()->message().view()), "tar: entry a.txt: a write past its size");
}

// A tar writer or reader moved from: the stream is the other's
TEST(CompressBoundaries_Tests, MovedTarWriterAndReader) {
    io::buffer out;
    tar::writer w(out);
    ASSERT_TRUE(w.write_header(tar_file("a", 2)));
    auto moved = std::move(w);
    EXPECT_FALSE(w.last_error());
    EXPECT_TRUE(w.close());
    EXPECT_FALSE(w.write_header(tar_file("lost", 0)));
    EXPECT_FALSE(w.write(std::string("x")));
    EXPECT_FALSE(w.close());
    auto& same = moved;
    moved = std::move(same);
    ASSERT_TRUE(moved.write(std::string("ab")));
    ASSERT_TRUE(moved.close());

    source src{contents(out)};
    auto closes = src.closes;
    tar::reader r{io::reader(src)};
    ASSERT_TRUE(r.next());
    auto m = std::move(r);
    auto n = r.next();
    ASSERT_FALSE(n);
    EXPECT_EQ(n.error().code(), compress::errc::io);
    byte b[4];
    auto rd = r.read(b);
    ASSERT_FALSE(rd);
    EXPECT_TRUE(rd.error().is_closed());
    EXPECT_TRUE(r.close());
    EXPECT_EQ(*closes, 0);
    auto& self = m;
    m = std::move(self);
    EXPECT_EQ(read_rest(m), "ab");
    auto end = m.next();
    ASSERT_TRUE(end);
    EXPECT_FALSE(*end);
    EXPECT_TRUE(m.close());
    EXPECT_EQ(*closes, 1);
}

// Every decompress in memory against max_size: the output exactly at it,
// and one byte past it; no output with a limit of 0
TEST(CompressBoundaries_Tests, DecompressAtTheSizeLimit) {
    const std::string plain = sample();
    const uint64_t n = plain.size();
    auto check = [&](const char* name, auto decompress) {
        SCOPED_TRACE(name);
        auto at = decompress(compress::limits{.max_size = n});
        ASSERT_TRUE(at);
        EXPECT_EQ(text(*at), plain);
        auto past = decompress(compress::limits{.max_size = n - 1});
        ASSERT_FALSE(past);
        EXPECT_EQ(past.error().code(), compress::errc::too_large);
        EXPECT_TRUE(decompress(compress::limits{UINT64_MAX}));
    };
    auto f = text(compress::flate::compress(bytes(plain)));
    auto z = text(compress::zlib::compress(bytes(plain)));
    auto g = text(compress::gzip::compress(bytes(plain)));
    auto g2 = text(compress::gzip::compress(bytes(plain.substr(0, 1000)))) + text(compress::gzip::compress(bytes(plain.substr(1000))));
    auto l = text(compress::lzma::compress(bytes(plain)));
    io::buffer ls;
    compress::lzma::writer lw(ls);
    ASSERT_TRUE(lw.write(plain));
    ASSERT_TRUE(lw.close());
    auto lu = contents(ls);   // its size not in the header
    auto x = text(compress::xz::compress(bytes(plain)));
    io::buffer xs;
    compress::xz::writer xw(xs);
    ASSERT_TRUE(xw.write(plain));
    ASSERT_TRUE(xw.close());
    auto xu = contents(xs);   // the block's sizes not in its header
    auto w = text(compress::lzw::compress(bytes(plain), compress::lzw::order::lsb, 8));
    auto b = bz2(plain);
    auto z4 = text(compress::lz4::compress(bytes(plain)));
    io::buffer z4s;
    compress::lz4::writer z4w(z4s, {.block_size = compress::lz4::block_size::kb64});
    ASSERT_TRUE(z4w.write(plain));
    ASSERT_TRUE(z4w.close());
    auto z4u = contents(z4s);   // its size not in the header
    check("lz4", [&](const compress::limits& lim) { return compress::lz4::decompress(bytes(z4), lim); });
    check("lz4 unsized", [&](const compress::limits& lim) { return compress::lz4::decompress(bytes(z4u), lim); });
    auto sn = text(compress::snappy::compress(bytes(plain)));
    auto snb = text(compress::snappy::compress_block(bytes(plain)));
    check("snappy", [&](const compress::limits& lim) { return compress::snappy::decompress(bytes(sn), lim); });
    check("snappy block", [&](const compress::limits& lim) { return compress::snappy::decompress_block(bytes(snb), lim); });
    auto zs = text(compress::zstd::compress(bytes(plain)));
    io::buffer zss;
    compress::zstd::writer zsw(zss);
    ASSERT_TRUE(zsw.write(plain));
    ASSERT_TRUE(zsw.close());
    auto zsu = contents(zss);   // its size not in the header
    check("zstd", [&](const compress::limits& lim) { return compress::zstd::decompress(bytes(zs), lim); });
    check("zstd unsized", [&](const compress::limits& lim) { return compress::zstd::decompress(bytes(zsu), lim); });
    auto br = text(compress::brotli::compress(bytes(plain), {.level = 4}));
    check("brotli", [&](const compress::limits& lim) { return compress::brotli::decompress(bytes(br), lim); });
    check("flate", [&](const compress::limits& lim) { return compress::flate::decompress(bytes(f), lim); });
    check("zlib", [&](const compress::limits& lim) { return compress::zlib::decompress(bytes(z), lim); });
    check("gzip", [&](const compress::limits& lim) { return compress::gzip::decompress(bytes(g), lim); });
    check("gzip twice", [&](const compress::limits& lim) { return compress::gzip::decompress(bytes(g2), lim); });
    check("lzma", [&](const compress::limits& lim) { return compress::lzma::decompress(bytes(l), lim); });
    check("lzma unsized", [&](const compress::limits& lim) { return compress::lzma::decompress(bytes(lu), lim); });
    check("xz", [&](const compress::limits& lim) { return compress::xz::decompress(bytes(x), lim); });
    check("xz unsized", [&](const compress::limits& lim) { return compress::xz::decompress(bytes(xu), lim); });
    check("lzw", [&](const compress::limits& lim) { return compress::lzw::decompress(bytes(w), compress::lzw::order::lsb, 8, lim); });
    check("bzip2", [&](const compress::limits& lim) { return compress::bzip2::decompress(bytes(b), lim); });
    // a limit of 0: the empty data passes, one byte does not
    const compress::limits zero{.max_size = 0};
    const std::string one = "x";
    EXPECT_TRUE(compress::flate::decompress(compress::flate::compress(""), zero));
    EXPECT_TRUE(compress::gzip::decompress(compress::gzip::compress(""), zero));
    EXPECT_TRUE(compress::lzma::decompress(compress::lzma::compress(""), zero));
    EXPECT_TRUE(compress::xz::decompress(compress::xz::compress(""), zero));
    EXPECT_TRUE(compress::lzw::decompress(compress::lzw::compress(bytes(std::string()), compress::lzw::order::lsb, 8), compress::lzw::order::lsb, 8, zero));
    EXPECT_TRUE(compress::bzip2::decompress(bytes(bz2("")), zero));
    EXPECT_TRUE(compress::lz4::decompress(compress::lz4::compress(""), zero));
    EXPECT_EQ(compress::lz4::decompress(compress::lz4::compress(bytes(one)), zero).error().code(), compress::errc::too_large);
    EXPECT_TRUE(compress::zstd::decompress(compress::zstd::compress(""), zero));
    EXPECT_EQ(compress::zstd::decompress(compress::zstd::compress(bytes(one)), zero).error().code(), compress::errc::too_large);
    EXPECT_TRUE(compress::brotli::decompress(compress::brotli::compress(""), zero));
    EXPECT_EQ(compress::brotli::decompress(compress::brotli::compress(bytes(one)), zero).error().code(), compress::errc::too_large);
    EXPECT_EQ(compress::flate::decompress(compress::flate::compress(bytes(one)), zero).error().code(), compress::errc::too_large);
    EXPECT_EQ(compress::gzip::decompress(compress::gzip::compress(bytes(one)), zero).error().code(), compress::errc::too_large);
    EXPECT_EQ(compress::lzma::decompress(compress::lzma::compress(bytes(one)), zero).error().code(), compress::errc::too_large);
    EXPECT_EQ(compress::xz::decompress(compress::xz::compress(bytes(one)), zero).error().code(), compress::errc::too_large);
    auto lzw_one = compress::lzw::compress(bytes(one), compress::lzw::order::lsb, 8);
    EXPECT_EQ(compress::lzw::decompress(lzw_one, compress::lzw::order::lsb, 8, zero).error().code(), compress::errc::too_large);
    EXPECT_EQ(compress::bzip2::decompress(bytes(bz2(one)), zero).error().code(), compress::errc::too_large);
}

// The ranges of the options: their ends taken, one past them refused
TEST(CompressBoundaries_Tests, OptionsAtTheirEnds) {
    using compress::level;
    for (int v : {0, 1, 6, 9, int(level::huffman_only)}) {
        EXPECT_EQ(level(v).value(), v);
    }
    for (int v : {-1, -3, 10, INT_MIN, INT_MAX}) {
        EXPECT_THROW(level{v}, std::invalid_argument);
    }
    EXPECT_EQ(level().value(), level::standard);
    // lzma: lc 0..8, lp and pb 0..4, a dictionary of 4 KiB to 1.5 GiB
    const std::string t = "abcabcabc";
    std::vector<compress::lzma::options> good(5), bad(5);
    good[0].lc = 8;
    good[1].lp = 4;
    good[2].pb = 4;
    good[3].dictionary = 4096;
    good[4].lc = 0;
    good[4].pb = 0;
    bad[0].lc = 9;
    bad[1].lp = 5;
    bad[2].pb = 5;
    bad[3].dictionary = 4095;
    bad[4].dictionary = (uint32_t(3) << 29) + 1;
    for (auto& o : good) {
        EXPECT_EQ(text(*compress::lzma::decompress(compress::lzma::compress(bytes(t), o))), t);
    }
    for (auto& o : bad) {
        EXPECT_THROW(compress::lzma::compress(bytes(t), o), std::invalid_argument);
        io::buffer out;
        compress::lzma::writer w(out, o);
        auto r = w.write(t);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), make_error_code(compress::errc::invalid_argument));
        EXPECT_EQ(out.size(), 0u);
    }
    // xz: Delta 1..256, the same dictionaries
    std::vector<compress::xz::options> xgood(3), xbad(3);
    xgood[0].delta = 1;
    xgood[1].delta = 256;
    xgood[2].dictionary = 4096;
    xbad[0].delta = 257;
    xbad[1].dictionary = 4095;
    xbad[2].dictionary = (uint32_t(3) << 29) + 1;
    for (auto& o : xgood) {
        EXPECT_EQ(text(*compress::xz::decompress(compress::xz::compress(bytes(t), o))), t);
    }
    for (auto& o : xbad) {
        EXPECT_THROW(compress::xz::compress(bytes(t), o), std::invalid_argument);
    }
    // lzw: a literal width of 2..8; the bytes it holds, and one past
    for (int width : {2, 8}) {
        std::string most(1, char((1 << width) - 1));
        auto packed = compress::lzw::compress(bytes(most), compress::lzw::order::lsb, width);
        EXPECT_EQ(text(*compress::lzw::decompress(packed, compress::lzw::order::lsb, width)), most);
    }
    EXPECT_THROW(compress::lzw::compress(bytes(std::string("\x04", 1)), compress::lzw::order::lsb, 2), std::invalid_argument);
    for (int width : {1, 9}) {
        EXPECT_THROW(compress::lzw::compress(bytes(t), compress::lzw::order::lsb, width), std::invalid_argument);
        EXPECT_THROW(compress::lzw::decompress(bytes(t), compress::lzw::order::lsb, width), std::invalid_argument);
        io::buffer out;
        EXPECT_THROW(compress::lzw::writer(out, compress::lzw::order::lsb, width), std::invalid_argument);
        EXPECT_THROW(compress::lzw::reader(io::reader(out), compress::lzw::order::lsb, width), std::invalid_argument);
    }
    // gzip: an extra field of 65535 bytes, the most; one more refused
    compress::gzip::options g;
    g.header.extra = sgcl::vector<byte>(65535, byte{7});
    auto packed = compress::gzip::compress(bytes(t), g);
    compress::gzip::reader r(io::reader(source{text(packed)}));
    auto h = r.header();
    ASSERT_TRUE(h);
    EXPECT_EQ(h->extra.size(), 65535u);
    g.header.extra = sgcl::vector<byte>(65536, byte{7});
    EXPECT_THROW(compress::gzip::compress(bytes(t), g), std::invalid_argument);
}

// The error's own ends: the default one, a code past the list, equality
TEST(CompressBoundaries_Tests, TheErrorAtItsEnds) {
    compress::error e;
    EXPECT_EQ(e.code(), compress::errc::corrupt);
    EXPECT_EQ(e.offset(), 0u);
    EXPECT_FALSE(e.io_error());
    EXPECT_EQ(std::string(e.message().view()), "offset 0: corrupt data");
    EXPECT_TRUE(e == e);
    EXPECT_TRUE(e == compress::error(compress::errc::corrupt, 0));
    compress::error far(compress::errc::checksum, UINT64_MAX, "x");
    EXPECT_EQ(std::string(far.message().view()), "offset 18446744073709551615: x");
    EXPECT_EQ(compress::compress_category().message(0), "unknown compress error");
    EXPECT_EQ(compress::compress_category().message(13), "unknown compress error");
    EXPECT_EQ(compress::compress_category().message(int(compress::errc::insecure_path)), "insecure path");
    EXPECT_EQ(std::string(compress::compress_category().name()), "compress");
    EXPECT_EQ(compress::make_error_code(compress::errc::io).value(), int(compress::errc::io));
}

// The task's forms of a stream moved from: the same as the thread's
TEST(CompressBoundaries_Tests, MovedStreamsInATask) {
    auto task = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        int ok = 0;
        io::buffer sink;
        compress::gzip::writer w(sink);
        auto wm = std::move(w);
        auto wr = co_await w.async_write(bytes(std::string("x")));
        ok += !wr && wr.error().is_closed();
        ok += bool(co_await wm.async_write(bytes(std::string("y"))));
        ok += bool(co_await wm.async_close());
        compress::xz::reader r(io::reader(source{contents(sink)}));
        auto rm = std::move(r);
        byte b[8];
        auto rr = co_await r.async_read(b);
        ok += !rr && rr.error().is_closed();
        tar::reader t(io::reader(source{std::string(1024, '\0')}));
        auto tm = std::move(t);
        ok += !(co_await t.async_next());
        auto end = co_await tm.async_next();
        ok += end && !*end;
        co_return ok;
    }());
    EXPECT_EQ(task.wait(), 6);
    sgcl::async::scheduler::stop();
}

// gzip's file functions at their ends: a file of no bytes both ways, a .gz
// of no bytes, a name that is ".gz" alone, a directory in place of a file;
// nothing half made is left behind a failure
TEST(CompressBoundaries_Tests, GzipFilesAtTheirEnds) {
    scratch_dir scratch("sgcl-gzf");
    const auto& s = scratch.path();
    put_file(s / "empty.txt", "");
    ASSERT_TRUE(compress::gzip::compress_file(path_of(s / "empty.txt"), {.keep = false}));
    EXPECT_FALSE(std::filesystem::exists(s / "empty.txt"));
    EXPECT_EQ(gunzip(slurp(s / "empty.txt.gz")), "");
    ASSERT_TRUE(compress::gzip::decompress_file(path_of(s / "empty.txt.gz")));
    EXPECT_TRUE(std::filesystem::exists(s / "empty.txt"));
    EXPECT_EQ(slurp(s / "empty.txt"), "");

    put_file(s / "none.gz", "");
    auto none = compress::gzip::decompress_file(path_of(s / "none.gz"));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), compress::errc::unexpected_end);
    EXPECT_FALSE(std::filesystem::exists(s / "none"));
    put_file(s / "one.gz", "\x1f");
    auto one = compress::gzip::decompress_file(path_of(s / "one.gz"));
    ASSERT_FALSE(one);
    EXPECT_EQ(one.error().code(), compress::errc::unexpected_end);
    EXPECT_FALSE(std::filesystem::exists(s / "one"));

    auto bare = compress::gzip::decompress_file(".gz");
    ASSERT_FALSE(bare);
    EXPECT_EQ(bare.error().code(), compress::errc::invalid_argument);
    EXPECT_EQ(bare.error().offset(), 0u);

    std::filesystem::create_directory(s / "dir");
    auto dir = compress::gzip::compress_file(path_of(s / "dir"));
    ASSERT_FALSE(dir);
    EXPECT_EQ(dir.error().code(), compress::errc::io);
    EXPECT_FALSE(std::filesystem::exists(s / "dir.gz"));
    EXPECT_FALSE(compress::gzip::compress_file(path_of(s / "missing")));
    EXPECT_FALSE(std::filesystem::exists(s / "missing.gz"));
}

namespace {
    // The files add_file is given: an empty one, one of a byte, one past a
    // copy's block, a link to one, a directory; with modes and a time
    struct added_files {
        scratch_dir scratch{"sgcl-add"};
        std::string big;
        sgcl::time::datetime when = sgcl::time::datetime::from_unix(1700000000, sgcl::time::zone::utc());

        added_files() {
            const auto& s = scratch.path();
            std::mt19937 rng(5);
            big.resize(300001);
            for (auto& ch : big) {
                ch = char(rng());
            }
            put_file(s / "empty.txt", "");
            put_file(s / "one.txt", "x");
            put_file(s / "big.bin", big);
            std::filesystem::create_symlink(s / "one.txt", s / "link.txt");
            std::filesystem::create_directory(s / "dir");
            EXPECT_TRUE(io::chmod(at("one.txt"), io::permissions(0600)));
            EXPECT_TRUE(io::chmod(at("big.bin"), io::permissions(0751)));
            for (auto n : {"empty.txt", "one.txt", "big.bin"}) {
                EXPECT_TRUE(io::set_modified(at(n), io::file_time(std::chrono::seconds(1700000000))));
            }
        }

        sgcl::string at(const char* name) const {
            return path_of(scratch.path() / name);
        }

        // Each writer asked the same: the files by their own names and
        // under one given, then what each refuses without keeping it
        template<class W>
        void add_all(W& w) {
            ASSERT_TRUE(w.add_file(at("empty.txt")));
            ASSERT_TRUE(w.add_file(at("one.txt")));
            ASSERT_TRUE(w.add_file(at("big.bin"), "data/big.bin"));
            ASSERT_TRUE(w.add_file(at("link.txt")));   // followed: the file's bytes
            auto missing = w.add_file(at("missing.txt"));
            ASSERT_FALSE(missing);
            EXPECT_EQ(missing.error().code(), compress::errc::io);
            ASSERT_TRUE(missing.error().io_error());
            EXPECT_TRUE(missing.error().io_error()->is_not_found());
            auto dir = w.add_file(at("dir"));
            ASSERT_FALSE(dir);
            EXPECT_EQ(dir.error().code(), compress::errc::invalid_argument);
            auto slash = w.add_file(at("one.txt"), "one/");
            ASSERT_FALSE(slash);
            EXPECT_EQ(slash.error().code(), compress::errc::invalid_argument);
            EXPECT_FALSE(w.add_file(""));
            EXPECT_FALSE(w.last_error());   // none of them is the archive's
        }
    };
}

// add_file on the three writers: the file's bytes, mode and time under its
// own name or one given, an empty file, a link followed; a missing path, a
// directory and a directory's name refused by the call alone; a writer
// closed or moved from refuses it as after a close; an output that fails
// half-way is the archive's error, kept
TEST(CompressBoundaries_Tests, WritersAddFiles) {
    added_files files;
    {
        io::buffer out;
        zip::writer w(out);
        files.add_all(w);
        ASSERT_TRUE(w.close());
        auto a = zip::archive::from(out.data());
        ASSERT_TRUE(a);
        ASSERT_EQ(a->entries().size(), 4u);
        std::vector<std::string> names;
        for (auto& e : a->entries()) {
            names.emplace_back(e.name.view());
        }
        EXPECT_EQ(names, (std::vector<std::string>{"empty.txt", "one.txt", "data/big.bin", "link.txt"}));
        EXPECT_EQ(text(*a->read("empty.txt")), "");
        EXPECT_EQ(text(*a->read("data/big.bin")), files.big);
        EXPECT_EQ(text(*a->read("link.txt")), "x");
        EXPECT_EQ(unsigned(a->entries()[1].mode), 0600u);
        EXPECT_EQ(unsigned(a->entries()[2].mode), 0751u);
        EXPECT_EQ(a->entries()[1].modified.unix(), files.when.unix());
        EXPECT_EQ(a->entries()[2].method, zip::method::deflate);
        auto late = w.add_file(files.at("one.txt"));
        ASSERT_FALSE(late);
        EXPECT_EQ(late.error().code(), compress::errc::invalid_argument);   // "zip: create after close", kept
        EXPECT_TRUE(w.last_error());
        zip::writer v(out);
        auto moved = std::move(v);
        EXPECT_FALSE(v.add_file(files.at("one.txt")));
    }
    {
        io::buffer out;
        tar::writer w(out);
        files.add_all(w);
        ASSERT_TRUE(w.close());
        tar::reader r{io::reader(out)};
        std::vector<std::string> names;
        while (auto e = r.next()) {
            if (!*e) {
                break;
            }
            names.emplace_back((*e)->name.view());
            std::string data = read_rest(r);
            if ((*e)->name == "data/big.bin") {
                EXPECT_EQ(data, files.big);
                EXPECT_EQ(unsigned((*e)->mode), 0751u);
                EXPECT_EQ((*e)->modified.unix(), files.when.unix());
            } else if ((*e)->name == "link.txt") {
                EXPECT_EQ(data, "x");
                EXPECT_EQ((*e)->type, tar::kind::file);
            }
        }
        EXPECT_EQ(names, (std::vector<std::string>{"empty.txt", "one.txt", "data/big.bin", "link.txt"}));
        auto late = w.add_file(files.at("one.txt"));
        ASSERT_FALSE(late);
        ASSERT_TRUE(late.error().io_error());
        EXPECT_TRUE(late.error().io_error()->is_closed());
        // in the middle of an entry: refused before the file is opened
        io::buffer mid;
        tar::writer m(mid);
        ASSERT_TRUE(m.write_header(tar_file("a", 2)));
        EXPECT_FALSE(m.add_file(files.at("one.txt")));
        EXPECT_TRUE(m.last_error());
        // an output that fails half-way through the file: the archive's error, kept
        failing_after sink(1000);
        tar::writer f{io::writer(sink)};
        auto failed = f.add_file(files.at("big.bin"));
        ASSERT_FALSE(failed);
        ASSERT_TRUE(f.last_error());
        EXPECT_TRUE(*f.last_error() == failed.error());
        EXPECT_FALSE(f.add_file(files.at("one.txt")));
        EXPECT_EQ(sink.failures, 1);
    }
    {
        io::buffer out;
        sevenzip::writer w(out);
        files.add_all(w);
        ASSERT_TRUE(w.close());
        auto a = sevenzip::archive::from(out.data());
        ASSERT_TRUE(a);
        ASSERT_EQ(a->entries().size(), 4u);
        EXPECT_EQ(std::string(a->entries()[2].name.view()), "data/big.bin");
        EXPECT_EQ(text(*a->read("data/big.bin")), files.big);
        EXPECT_EQ(text(*a->read("link.txt")), "x");
        EXPECT_EQ(text(*a->read("empty.txt")), "");
        EXPECT_EQ((a->entries()[1].attributes >> 16) & 07777, 0600u);
        ASSERT_TRUE(a->entries()[1].modified);
        EXPECT_EQ(a->entries()[1].modified->unix(), files.when.unix());
        auto late = w.add_file(files.at("one.txt"));
        ASSERT_FALSE(late);
        ASSERT_TRUE(w.last_error());
        sevenzip::writer v(out);
        auto moved = std::move(v);
        EXPECT_FALSE(v.add_file(files.at("one.txt")));
    }
    // zip's output failing half-way: kept, as tar's
    failing_after sink(1000);
    zip::writer z{io::writer(sink)};
    auto failed = z.add_file(files.at("big.bin"));
    ASSERT_FALSE(failed);
    ASSERT_TRUE(z.last_error());
    EXPECT_FALSE(z.add_file(files.at("one.txt")));
}
