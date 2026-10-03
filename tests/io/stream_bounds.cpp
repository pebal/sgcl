//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The streams of stream.h and the algorithms of functions.h at their
// boundaries (DESIGN 408): an empty stream, a read or a write of zero bytes,
// the end at every position, a failure part way, a stream given itself, the
// limits of a position and of a string, a handle moved from.
#include "tests/types.h"

using namespace sgcl::async;

#include <climits>
#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    std::string text_of(const slice<const byte>& b) {
        return std::string(reinterpret_cast<const char*>(b.data()), b.size());
    }

    // A reader of the text that counts its reads, and fails after
    // `fail_after` bytes when that is set
    class counted final : public io::mixin::reader<counted> {
    public:
        explicit counted(std::string_view s, size_t fail_after = SIZE_MAX) : _s(s), _fail_after(fail_after) {}

        expected<size_t, io::error> read(const slice<byte>& out) {
            ++reads;
            if (_given >= _fail_after) {
                return sgcl::unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "counted"));
            }
            size_t k = std::min({out.size(), _s.size(), _fail_after - _given});
            std::memcpy(out.data(), _s.data(), k);
            _s.remove_prefix(k);
            _given += k;
            return k;
        }

        task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

        size_t reads = 0;

    private:
        std::string_view _s;
        size_t _fail_after;
        size_t _given = 0;
    };

    // A writer that keeps what it is given and fails once it holds `room` bytes
    class bounded final : public io::mixin::writer<bounded> {
    public:
        using io::mixin::writer<bounded>::write;

        explicit bounded(size_t room = SIZE_MAX) : _room(room) {}

        expected<size_t, io::error> write(const slice<const byte>& data) {
            ++writes;
            if (got.size() + data.size() > _room) {
                return sgcl::unexpected(io::error(std::make_error_code(std::errc::no_space_on_device), "write", "bounded"));
            }
            got.append(reinterpret_cast<const char*>(data.data()), data.size());
            return data.size();
        }

        task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            co_return write(data);
        }

        std::string got;
        size_t writes = 0;

    private:
        size_t _room;
    };

    // A reader that claims bytes without writing them: a stream as long as
    // asked, whose bytes nobody looks at
    struct claiming {
        uint64_t left;

        expected<size_t, io::error> read(const slice<byte>& out) {
            size_t k = size_t(std::min<uint64_t>(out.size(), left));
            left -= k;
            return k;
        }
    };
}

// A buffer moved from is still the same buffer: a buffer always has its
// state, and a move copies the word (a moved-from handle was a null word,
// and every member read through it)
TEST(IoStreamBounds_Tests, ABufferMovedFromIsTheSameBuffer) {
    buffer a("x");
    buffer b = std::move(a);
    ASSERT_TRUE(a.write("y"));
    EXPECT_EQ(b.text(), "xy");
    EXPECT_TRUE(a == b);
    buffer c;
    c = std::move(b);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(c.text(), "xy");
    EXPECT_TRUE(b == c);
}

// A default buffer and an empty one at every member: zero bytes read and
// written, the end, no position before the first byte
TEST(IoStreamBounds_Tests, AnEmptyBufferAtEveryMember) {
    buffer b;
    byte out[4];
    EXPECT_EQ(value_of(b.read(out)), 0u);
    EXPECT_EQ(value_of(b.read(slice<byte>())), 0u);
    EXPECT_EQ(value_of(b.write(slice<const byte>())), 0u);
    EXPECT_EQ(value_of(b.write("")), 0u);
    EXPECT_TRUE(b.empty());
    EXPECT_TRUE(b.data().empty());
    EXPECT_EQ(b.text(), "");
    EXPECT_TRUE(b.release().empty());
    EXPECT_EQ(value_of(b.tell()), 0u);
    EXPECT_EQ(value_of(b.seek(0, seek_from::end)), 0u);
    EXPECT_EQ(error_of(b.seek(-1, seek_from::end)).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(b.seek(-1, seek_from::current)).code(), std::errc::invalid_argument);
    b.clear();
    b.reserve(0);
    EXPECT_TRUE(b.read_all());
    EXPECT_EQ(value_of(b.read_all_text()), "");
    bounded w;
    EXPECT_EQ(value_of(io::copy(w, b)), 0u);
    EXPECT_EQ(w.got, "");
    // a read of nothing from a buffer that holds bytes takes none
    ASSERT_TRUE(b.write("ab"));
    EXPECT_EQ(value_of(b.read(slice<byte>())), 0u);
    EXPECT_EQ(b.text(), "ab");
    // one byte
    EXPECT_EQ(value_of(b.read(slice<byte>(out, 1))), 1u);
    EXPECT_EQ(b.text(), "b");
    EXPECT_EQ(value_of(b.read(out)), 1u);
    EXPECT_EQ(value_of(b.read(out)), 0u);
}

// A buffer's own bytes written into it: at the end appended, at a position
// in the middle written as they were before the write (pwrite of the
// bytes it was given), overlapping or not
TEST(IoStreamBounds_Tests, ABufferWrittenWithItsOwnBytes) {
    buffer b("abcd");
    ASSERT_TRUE(b.write(b.data()));
    EXPECT_EQ(b.text(), "abcdabcd");

    buffer c("abcd");
    ASSERT_TRUE(c.seek(1));
    EXPECT_EQ(value_of(c.write(c.data())), 4u);
    EXPECT_EQ(c.text(), "aabcd");
    EXPECT_EQ(value_of(c.tell()), 5u);

    buffer d("abcd");
    ASSERT_TRUE(d.seek(0));
    ASSERT_TRUE(d.write(d.data()));
    EXPECT_EQ(d.text(), "abcd");

    buffer e("abcd");
    ASSERT_TRUE(e.seek(2));
    ASSERT_TRUE(e.write(e.data().first(3)));
    EXPECT_EQ(e.text(), "ababc");

    buffer f("abcdef");
    ASSERT_TRUE(f.seek(0));
    ASSERT_TRUE(f.write(f.data().subspan(2)));   // a later part of itself, over its front
    EXPECT_EQ(f.text(), "cdefef");

    buffer g("xabcd");   // a byte read first: the position counts from the first byte held
    byte one[1];
    ASSERT_EQ(value_of(g.read(one)), 1u);
    ASSERT_TRUE(g.seek(1));
    ASSERT_TRUE(g.write(g.data()));
    EXPECT_EQ(g.text(), "aabcd");

    buffer h("ab");   // past the end, the gap zeros
    ASSERT_TRUE(h.seek(3));
    ASSERT_TRUE(h.write(h.data()));
    EXPECT_EQ(h.text(), std::string("ab\0ab", 5));
}

// A buffer copied into itself, as Go's io.Copy(b, b) does it: what it held
// is taken and written at its end, once, so it holds the same bytes
TEST(IoStreamBounds_Tests, ABufferCopiedIntoItself) {
    buffer b("abc");
    EXPECT_EQ(value_of(io::copy(b, b)), 3u);
    EXPECT_EQ(b.text(), "abc");
    EXPECT_EQ(value_of(b.write_to(b)), 3u);
    EXPECT_EQ(b.text(), "abc");
    auto n = spawn([](buffer x) -> task<expected<size_t, io::error>> {
        co_return co_await io::async_copy(x, x);
    }(b)).wait();
    EXPECT_EQ(value_of(n), 3u);
    EXPECT_EQ(b.text(), "abc");
    buffer same = b;
    same = b;
    b = same;
    EXPECT_EQ(b.text(), "abc");
    EXPECT_TRUE(b == same);
}

// A buffer's write_to into a writer that fails keeps the bytes; one that
// succeeds empties the buffer
TEST(IoStreamBounds_Tests, AFailedWriteToKeepsTheBytes) {
    buffer b("abcdef");
    bounded full(2);
    EXPECT_EQ(error_of(b.write_to(full)).code(), std::errc::no_space_on_device);
    EXPECT_EQ(b.text(), "abcdef");
    EXPECT_EQ(error_of(io::copy(full, b)).code(), std::errc::no_space_on_device);
    EXPECT_EQ(b.text(), "abcdef");
    auto r = spawn([](buffer x, bounded& w) -> task<expected<size_t, io::error>> {
        co_return co_await x.async_write_to(w);
    }(b, full)).wait();
    EXPECT_FALSE(r);
    EXPECT_EQ(b.text(), "abcdef");
    bounded room;
    EXPECT_EQ(value_of(b.write_to(room)), 6u);
    EXPECT_EQ(room.got, "abcdef");
    EXPECT_TRUE(b.empty());
}

// The limits of a position: a seek whose sum passes int64_t is refused and
// moves nothing, as one before the first byte; a write that would make the
// buffer pass the largest vector is length_error, the buffer as it was
TEST(IoStreamBounds_Tests, ABuffersPositionAtItsLimits) {
    buffer b("abc");
    ASSERT_TRUE(b.seek(1));
    EXPECT_EQ(error_of(b.seek(INT64_MAX, seek_from::current)).code(), std::errc::value_too_large);
    EXPECT_EQ(error_of(b.seek(INT64_MAX, seek_from::end)).code(), std::errc::value_too_large);
    EXPECT_EQ(error_of(b.seek(INT64_MIN, seek_from::end)).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(b.seek(INT64_MIN)).code(), std::errc::invalid_argument);
    EXPECT_EQ(value_of(b.tell()), 1u);
    EXPECT_EQ(value_of(b.seek(INT64_MAX)), uint64_t(INT64_MAX));
    EXPECT_THROW((void)b.write("x"), std::length_error);
    EXPECT_EQ(b.text(), "abc");
    EXPECT_THROW(b.reserve(SIZE_MAX), std::length_error);
    EXPECT_EQ(b.text(), "abc");
    ASSERT_TRUE(b.seek(0, seek_from::end));
    ASSERT_TRUE(b.write("d"));
    EXPECT_EQ(b.text(), "abcd");
}

// An empty reader and writer: close, the observers and a comparison work;
// a moved-from one is a copy (the three words), still the same stream and
// keeping it alive
TEST(IoStreamBounds_Tests, EmptyAndMovedReadersAndWriters) {
    io::reader r;
    io::writer w;
    EXPECT_FALSE(r);
    EXPECT_FALSE(w);
    EXPECT_TRUE(r.close());
    EXPECT_TRUE(w.close());
    EXPECT_TRUE(spawn(r.async_close()).wait());
    EXPECT_TRUE(spawn(w.async_close()).wait());
    EXPECT_FALSE(r.has_read() || r.has_async_read() || r.has_close());
    EXPECT_FALSE(w.has_write() || w.has_async_write() || w.has_close());
    EXPECT_EQ(r.fd(), -1);
    EXPECT_EQ(w.fd(), -1);
    EXPECT_TRUE(r == io::reader());
    EXPECT_TRUE(w == io::writer());

    buffer b("text");
    io::reader from = b;
    io::reader to = std::move(from);
    EXPECT_TRUE(from);
    EXPECT_TRUE(from == to);
    io::writer wf = b;
    io::writer wt;
    wt = std::move(wf);
    EXPECT_TRUE(wf);
    EXPECT_TRUE(wf == wt);
    ASSERT_TRUE(wf.write("!"));
    EXPECT_EQ(value_of(from.read_all_text()), "text!");
}

// A reader moved from keeps the stream it was given alive: a callable's box
// is held by its words, which the move copies (the move used to take the
// owner and leave the pointer, a reader of a stream nothing kept)
TEST(IoStreamBounds_Tests, AMovedFromReaderKeepsItsStream) {
    static std::atomic<int> dropped = 0;
    struct source {
        source() = default;
        source(const source&) = default;
        ~source() {
            ++dropped;
        }
        expected<size_t, io::error> read(const slice<byte>& out) {
            if (out.empty()) {
                return 0;
            }
            out[0] = byte('z');
            return 1;
        }
    };
    auto holder = make_tracked<io::reader>();   // the reader in a managed object: traced precisely, no stack word keeps the box
    off_frame([&] {
        *holder = io::reader(source());
        io::reader taken = std::move(*holder);   // holder is the reader moved from
        (void)taken;
    });
    int before = dropped.load();
    collector::clear_stack();
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(dropped.load(), before);   // the box is still held by the reader in holder
    EXPECT_TRUE(*holder);
    byte one[1];
    EXPECT_EQ(value_of(holder->read(one)), 1u);
    EXPECT_EQ(one[0], byte('z'));
}

// limit_reader: a limit of 0 and an empty buffer read nothing from the
// source; the limit counts only what was read; an error leaves it
TEST(IoStreamBounds_Tests, LimitReaderAtItsBounds) {
    counted src("abcdef", 4);
    limit_reader none(src, 0);
    byte out[8];
    EXPECT_EQ(value_of(none.read(out)), 0u);
    EXPECT_EQ(src.reads, 0u);
    limit_reader l(src, 5);
    EXPECT_EQ(value_of(l.read(slice<byte>())), 0u);
    EXPECT_EQ(src.reads, 0u);
    EXPECT_EQ(value_of(l.read(slice<byte>(out, 3))), 3u);
    EXPECT_EQ(l.remaining(), 2u);
    EXPECT_EQ(value_of(l.read(out)), 1u);   // the source fails after 4 bytes: it gives 1 first
    EXPECT_EQ(l.remaining(), 1u);
    EXPECT_FALSE(l.read(out));
    EXPECT_EQ(l.remaining(), 1u);
    limit_reader huge(buffer("ab"), UINT64_MAX);
    EXPECT_EQ(value_of(huge.read_all_text()), "ab");
    EXPECT_EQ(huge.remaining(), UINT64_MAX - 2);
    limit_reader exact(buffer("abc"), 3);
    EXPECT_EQ(value_of(exact.read_all_text()), "abc");
    EXPECT_EQ(value_of(exact.read(out)), 0u);
}

// tee_reader: nothing read, nothing written; a read's error writes nothing;
// a write's error is the read's
TEST(IoStreamBounds_Tests, TeeReaderAtItsBounds) {
    bounded w;
    counted src("abc", 2);
    tee_reader t(src, w);
    byte out[8];
    EXPECT_EQ(value_of(t.read(slice<byte>())), 0u);
    EXPECT_EQ(w.writes, 0u);
    EXPECT_EQ(value_of(t.read(out)), 2u);
    EXPECT_EQ(w.got, "ab");
    EXPECT_FALSE(t.read(out));
    EXPECT_EQ(w.writes, 1u);
    bounded full(1);
    tee_reader u(buffer("xyz"), full);
    EXPECT_EQ(error_of(u.read(out)).code(), std::errc::no_space_on_device);
}

// multi_reader: none, empty ones, an empty buffer, an error in the middle
// (the next read asks the same reader again)
TEST(IoStreamBounds_Tests, MultiReaderAtItsBounds) {
    multi_reader none{vector<io::reader>()};
    byte out[8];
    EXPECT_EQ(value_of(none.read(out)), 0u);
    multi_reader empties{vector<io::reader>{buffer(), buffer(), buffer("a"), buffer()}};
    EXPECT_EQ(value_of(empties.read(slice<byte>())), 0u);
    EXPECT_EQ(value_of(empties.read_all_text()), "a");
    EXPECT_EQ(value_of(empties.read(out)), 0u);
    counted bad("ab", 1);
    multi_reader m{vector<io::reader>{buffer("x"), io::reader(bad), buffer("y")}};
    EXPECT_EQ(value_of(m.read(out)), 1u);
    EXPECT_EQ(value_of(m.read(out)), 1u);
    EXPECT_FALSE(m.read(out));
    EXPECT_FALSE(m.read(out));   // the failing reader again, not skipped
    auto all = spawn([](multi_reader& r) -> task<expected<string, io::error>> {
        co_return co_await r.async_read_all_text();
    }(empties)).wait();
    EXPECT_EQ(value_of(all), "");
}

// multi_writer: none takes everything; zero bytes reach each writer; the
// first error stops the rest
TEST(IoStreamBounds_Tests, MultiWriterAtItsBounds) {
    multi_writer none{vector<io::writer>()};
    EXPECT_EQ(value_of(none.write("abc")), 3u);
    bounded a, b(1), c;
    multi_writer m{vector<io::writer>{io::writer(a), io::writer(b), io::writer(c)}};
    EXPECT_EQ(value_of(m.write("")), 0u);
    EXPECT_EQ(a.writes + b.writes + c.writes, 3u);
    EXPECT_EQ(error_of(m.write("xy")).code(), std::errc::no_space_on_device);
    EXPECT_EQ(a.got, "xy");
    EXPECT_EQ(c.writes, 1u);
    discard_writer d;
    EXPECT_EQ(value_of(d.write(slice<const byte>())), 0u);
    EXPECT_EQ(value_of(io::copy(io::discard, buffer())), 0u);
}

// transform_reader: the function is not called for nothing read or an error
TEST(IoStreamBounds_Tests, TransformReaderAtItsBounds) {
    int calls = 0;
    counted src("ab", 1);
    transform_reader t(io::reader(src), [&](const slice<byte>&) { ++calls; });
    byte out[4];
    EXPECT_EQ(value_of(t.read(slice<byte>())), 0u);
    EXPECT_EQ(value_of(t.read(out)), 1u);
    EXPECT_FALSE(t.read(out));
    EXPECT_EQ(calls, 1);
}

// read_full at every end: a stream of k bytes into a buffer of n, for each
// k; an empty buffer reads nothing; an error part way is the read's own
TEST(IoStreamBounds_Tests, ReadFullEndsAtEveryPosition) {
    const std::string text = "abcdefgh";
    for (size_t n = 0; n <= text.size(); ++n) {
        for (size_t k = 0; k <= text.size(); ++k) {
            counted src(std::string_view(text).substr(0, k));
            byte out[8] = {};
            auto r = io::read_full(src, slice<byte>(out, n));
            if (n == 0) {
                EXPECT_EQ(value_of(r), 0u);
                EXPECT_EQ(src.reads, 0u);
            } else if (k == 0) {
                EXPECT_EQ(value_of(r), 0u) << n << " " << k;
            } else if (k < n) {
                ASSERT_FALSE(r) << n << " " << k;
                EXPECT_TRUE(r.error().is_eof());
                EXPECT_EQ(r.error().count(), k);
                EXPECT_EQ(std::string(reinterpret_cast<char*>(out), k), text.substr(0, k));
            } else {
                EXPECT_EQ(value_of(r), n) << n << " " << k;
            }
            buffer b(slice<const byte>(reinterpret_cast<const byte*>(text.data()), k));
            byte again[8];
            auto a = spawn(b.async_read_full(slice<byte>(again, n))).wait();
            EXPECT_EQ(bool(a), n == 0 || k == 0 || k >= n);
        }
    }
    counted bad("abcdef", 3);
    byte out[6];
    auto r = io::read_full(bad, out);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::io_error);   // the read's error, not unexpected_eof
}

// read_all and read_all_text: an empty stream, the block's size and one
// past it, an error part way
TEST(IoStreamBounds_Tests, ReadAllAtTheBlocksEdges) {
    EXPECT_TRUE(value_of(io::read_all(buffer())).empty());
    for (size_t n : {size_t(1), config::io_buffer_size - 1, config::io_buffer_size, config::io_buffer_size + 1, 2 * config::io_buffer_size}) {
        std::string s(n, 'q');
        counted src(s);
        EXPECT_EQ(value_of(io::read_all(src)).size(), n);
        counted src2(s);
        EXPECT_EQ(value_of(io::read_all_text(src2)).size(), n);
        buffer b(string(s.c_str()));
        auto a = spawn(b.async_read_all()).wait();
        EXPECT_EQ(value_of(a).size(), n);
    }
    std::string s(3 * config::io_buffer_size, 'q');
    counted bad(s, config::io_buffer_size + 5);
    EXPECT_EQ(error_of(io::read_all(bad)).code(), std::errc::io_error);
    counted bad2(s, 7);
    EXPECT_EQ(error_of(io::read_all_text(bad2)).code(), std::errc::io_error);
}

// The 4 GiB of a string: a stream one byte longer than a string holds is
// length_error from read_all_text, and the gathered memory goes with it
// (the bytes are claimed, never written: the pages are never touched)
TEST(IoStreamBounds_Tests, ReadAllTextPastTheLargestString) {
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    GTEST_SKIP() << "8 GiB of address space under a sanitizer";
#endif
#endif
    claiming past{uint64_t(UINT32_MAX) + 1};
    EXPECT_THROW((void)io::read_all_text(past), std::length_error);
    EXPECT_EQ(past.left, 0u);
}

// copy: an empty source writes nothing; a write's error part way leaves
// what was written; a read's error after a write is the read's
TEST(IoStreamBounds_Tests, CopyFailsPartWay) {
    bounded w;
    counted empty("");
    EXPECT_EQ(value_of(io::copy(w, empty)), 0u);
    EXPECT_EQ(w.writes, 0u);
    std::string big(3 * config::io_copy_buffer_size, 'x');
    counted src(big);
    bounded small(config::io_copy_buffer_size + 1);
    EXPECT_EQ(error_of(io::copy(small, src)).code(), std::errc::no_space_on_device);
    EXPECT_EQ(small.got.size(), config::io_copy_buffer_size);
    counted bad(big, config::io_copy_buffer_size + 3);
    bounded all;
    EXPECT_EQ(error_of(io::copy(all, bad)).code(), std::errc::io_error);
    EXPECT_EQ(all.got.size(), config::io_copy_buffer_size + 3);
    // zero bytes written through write()
    bounded z;
    EXPECT_EQ(value_of(io::write(z, "")), 0u);
    EXPECT_EQ(value_of(io::write(z, slice<const byte>())), 0u);
}
