//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// buffered_reader and buffered_writer at their boundaries (DESIGN 408): an
// empty and a moved-from handle, an empty stream, the end at every byte,
// lines at the size of the block and of the bound, zero bytes, a failure
// part way, a closed stream underneath.
#include "tests/types.h"

using namespace sgcl::async;

#include <string>
#include <string_view>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    // A reader of the text that fails after `fail_after` bytes
    class breaking final : public io::mixin::reader<breaking> {
    public:
        breaking(std::string s, size_t fail_after) : _s(std::move(s)), _fail_after(fail_after) {}

        expected<size_t, io::error> read(const slice<byte>& out) {
            ++reads;
            if (_pos >= _fail_after) {
                return sgcl::unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "breaking"));
            }
            size_t k = std::min({out.size(), _s.size() - _pos, _fail_after - _pos});
            std::memcpy(out.data(), _s.data() + _pos, k);
            _pos += k;
            return k;
        }

        task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

        int reads = 0;

    private:
        std::string _s;
        size_t _pos = 0;
        size_t _fail_after;
    };

    // A writer that keeps what it is given, fails past `room`, counts its
    // writes and closes
    class sink final : public io::mixin::writer<sink> {
    public:
        using io::mixin::writer<sink>::write;
        using io::mixin::writer<sink>::async_write;

        explicit sink(size_t room = SIZE_MAX) : _room(room) {}

        expected<size_t, io::error> write(const slice<const byte>& data) {
            ++writes;
            if (text.size() + data.size() > _room) {
                return sgcl::unexpected(io::error(std::make_error_code(std::errc::no_space_on_device), "write", "sink"));
            }
            text.append(reinterpret_cast<const char*>(data.data()), data.size());
            return data.size();
        }

        task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            co_return write(data);
        }

        expected<void, io::error> close() {
            ++closes;
            return {};
        }

        std::string text;
        int writes = 0;
        int closes = 0;

    private:
        size_t _room;
    };

    std::string str(const slice<const char>& s) {
        return std::string(s.data(), s.size());
    }

    std::string str(const slice<const byte>& s) {
        return std::string(reinterpret_cast<const char*>(s.data()), s.size());
    }
}

// An empty handle holds no state; one moved from still holds its own, a
// move of a tracked word being a copy
TEST(IoBufferedBounds_Tests, EmptyAndMovedFromHandles) {
    buffered_reader r;
    buffered_writer w;
    EXPECT_FALSE(r);
    EXPECT_FALSE(w);
    buffered_reader a(buffer("x"));
    buffered_reader b = std::move(a);
    EXPECT_TRUE(a);
    EXPECT_TRUE(a == b);
    buffered_writer c(buffer{});
    buffered_writer d;
    d = std::move(c);
    EXPECT_TRUE(c);
    EXPECT_TRUE(c == d);
    EXPECT_FALSE(io::reader(r));   // an empty handle makes an empty stream
    EXPECT_FALSE(io::writer(w));
}

// A reader over an empty stream at every member
TEST(IoBufferedBounds_Tests, AReaderOfAnEmptyStream) {
    buffered_reader r(buffer{});
    byte out[4];
    EXPECT_EQ(value_of(r.read(out)), 0u);
    EXPECT_EQ(value_of(r.read(slice<byte>())), 0u);
    EXPECT_FALSE(value_of(r.read_line()));
    EXPECT_FALSE(value_of(r.read_until(',')));
    EXPECT_TRUE(value_of(r.peek(0)).empty());
    EXPECT_TRUE(value_of(r.peek(10)).empty());
    EXPECT_FALSE(value_of(r.read_byte()));
    EXPECT_EQ(value_of(r.discard(0)), 0u);
    EXPECT_EQ(value_of(r.discard(5)), 0u);
    EXPECT_EQ(r.buffered(), 0u);
    int n = 0;
    for (auto line : r.lines()) {
        (void)line;
        ++n;
    }
    EXPECT_EQ(n, 0);
    EXPECT_FALSE(r.last_error());
    EXPECT_FALSE(value_of(spawn(r.async_read_line()).wait()));
    EXPECT_FALSE(value_of(spawn(r.async_read_until('x')).wait()));
    EXPECT_EQ(value_of(spawn(r.async_read(slice<byte>(out, 4))).wait()), 0u);
    EXPECT_EQ(value_of(r.read_all_text()), "");
    EXPECT_TRUE(r.close());
}

// One byte, one delimiter, and a lone "\r": the last token without its
// delimiter is a token, a line loses its "\r"
TEST(IoBufferedBounds_Tests, TheSmallestStreams) {
    buffered_reader a(buffer("a"));
    EXPECT_EQ(str(*value_of(a.read_line())), "a");
    EXPECT_FALSE(value_of(a.read_line()));
    buffered_reader nl(buffer("\n"));
    EXPECT_EQ(str(*value_of(nl.read_line())), "");
    EXPECT_FALSE(value_of(nl.read_line()));
    buffered_reader crlf(buffer("\r\n"));
    EXPECT_EQ(str(*value_of(crlf.read_line())), "");
    EXPECT_FALSE(value_of(crlf.read_line()));
    buffered_reader cr(buffer("\r"));
    EXPECT_EQ(str(*value_of(cr.read_line())), "");
    EXPECT_FALSE(value_of(cr.read_line()));
    buffered_reader comma(buffer(","));
    EXPECT_EQ(str(*value_of(comma.read_until(','))), ",");
    EXPECT_FALSE(value_of(comma.read_until(',')));
    buffered_reader zero(buffer(slice<const byte>(reinterpret_cast<const byte*>("a\0b"), 3)));
    EXPECT_EQ(value_of(zero.read_until('\0'))->size(), 2u);
    EXPECT_EQ(str(*value_of(zero.read_until('\0'))), "b");
}

// read_byte to the end of a stream of each length, and the end again
TEST(IoBufferedBounds_Tests, ReadByteEndsAtEveryPosition) {
    for (size_t k = 0; k <= 4; ++k) {
        std::string s = std::string("wxyz").substr(0, k);
        buffered_reader r(buffer(string(s.c_str())));
        for (size_t i = 0; i < k; ++i) {
            auto b = value_of(r.read_byte());
            ASSERT_TRUE(b);
            EXPECT_EQ(char(*b), s[i]);
        }
        EXPECT_FALSE(value_of(r.read_byte()));
        EXPECT_FALSE(value_of(r.read_byte()));
        EXPECT_FALSE(value_of(r.read_line()));
    }
}

// Lines at the size of the block (8 KB) and around it, as a line, with the
// bound set at their length and one under it
TEST(IoBufferedBounds_Tests, LinesAtTheBlocksSize) {
    const size_t block = config::io_buffer_size;
    for (size_t len : {block - 2, block - 1, block, block + 1, 2 * block}) {
        std::string line(len, 'L');
        buffered_reader r(buffer(string((line + "\nnext\n").c_str())));
        EXPECT_EQ(value_of(r.read_line())->size(), len) << len;
        EXPECT_EQ(str(*value_of(r.read_line())), "next");
        EXPECT_FALSE(value_of(r.read_line()));

        buffered_reader exact(buffer(string((line + "\nnext\n").c_str())));
        exact.set_max_line(len);
        EXPECT_EQ(value_of(exact.read_line())->size(), len) << len;

        buffered_reader under(buffer(string((line + "\nnext\n").c_str())));
        under.set_max_line(len - 1);
        EXPECT_EQ(error_of(under.read_line()).code(), errc::line_too_long) << len;
        EXPECT_EQ(str(*value_of(under.read_line())), "next") << len;   // skipped whole

        buffered_reader last(buffer(string(line.c_str())));   // the last token, no delimiter
        EXPECT_EQ(value_of(last.read_line())->size(), len);
        EXPECT_FALSE(value_of(last.read_line()));
    }
    buffered_reader huge(buffer("abc\n"));
    huge.set_max_line(SIZE_MAX);
    EXPECT_EQ(huge.max_line(), SIZE_MAX);
    EXPECT_EQ(str(*value_of(huge.read_line())), "abc");
}

// peek at its bounds: none, the block's worth however many are asked, the
// end
TEST(IoBufferedBounds_Tests, PeekAtItsBounds) {
    std::string s(3 * config::io_buffer_size, 'p');
    buffered_reader r(buffer(string(s.c_str())));
    EXPECT_TRUE(value_of(r.peek(0)).empty());
    EXPECT_EQ(value_of(r.peek(SIZE_MAX)).size(), config::io_buffer_size);
    EXPECT_EQ(value_of(r.discard(config::io_buffer_size - 1)), config::io_buffer_size - 1);
    EXPECT_EQ(value_of(r.peek(config::io_buffer_size)).size(), config::io_buffer_size);   // the one left moved to the front, more read behind it
    EXPECT_EQ(value_of(r.discard(SIZE_MAX)), 2 * config::io_buffer_size + 1);
    EXPECT_TRUE(value_of(r.peek(1)).empty());
}

// A read that fails part way: the error, at each operation; what was read
// before it stays buffered
TEST(IoBufferedBounds_Tests, AReaderFailsPartWay) {
    breaking src("abc\ndef", 6);
    buffered_reader r(src);
    EXPECT_EQ(str(*value_of(r.read_line())), "abc");
    EXPECT_EQ(error_of(r.read_line()).code(), std::errc::io_error);
    EXPECT_EQ(r.buffered(), 2u);   // "de" read before the failure, still there
    EXPECT_EQ(str(value_of(r.peek(2))), "de");
    EXPECT_EQ(error_of(r.peek(3)).code(), std::errc::io_error);
    EXPECT_EQ(error_of(r.discard(3)).code(), std::errc::io_error);
    EXPECT_FALSE(r.read_byte());

    breaking src2("one\ntwo\nthree", 9);
    buffered_reader l(src2);
    std::string got;
    for (auto line : l.lines()) {
        got += str(line) + "|";
    }
    EXPECT_EQ(got, "one|two|");
    ASSERT_TRUE(l.last_error());
    EXPECT_EQ(l.last_error()->code(), std::errc::io_error);

    breaking direct(std::string(2 * config::io_buffer_size, 'd'), 0);
    buffered_reader big(direct);
    std::string room(config::io_buffer_size, '\0');
    EXPECT_FALSE(big.read(slice<byte>(reinterpret_cast<byte*>(room.data()), room.size())));
}

// A reader over a file closed under it: errc::closed; its own close closes
// the file once, a second close succeeds
TEST(IoBufferedBounds_Tests, AReaderOverAClosedStream) {
    auto p = pipe();
    ASSERT_TRUE(p);
    ASSERT_TRUE(p->write.write("ab\ncd"));
    buffered_reader r(p->read);
    EXPECT_EQ(str(*value_of(r.read_line())), "ab");
    ASSERT_TRUE(r.close());
    EXPECT_TRUE(p->read.is_closed());
    EXPECT_EQ(r.buffered(), 0u);   // what was buffered is dropped
    EXPECT_EQ(error_of(r.read_line()).code(), errc::closed);
    EXPECT_TRUE(r.close());
    ASSERT_TRUE(p->write.close());
}

// The writer at its bounds: zero bytes, the block's size exactly, a flush of
// nothing, two closes, a write after the close
TEST(IoBufferedBounds_Tests, AWriterAtItsBounds) {
    sink s;
    buffered_writer w(s);
    EXPECT_EQ(value_of(w.write(slice<const byte>())), 0u);
    EXPECT_EQ(value_of(w.write("")), 0u);
    EXPECT_EQ(w.buffered(), 0u);
    EXPECT_TRUE(w.flush());
    EXPECT_EQ(s.writes, 0);
    std::string block(config::io_buffer_size, 'b');
    EXPECT_EQ(value_of(w.write(block)), block.size());   // an empty block and a whole block's worth: straight through
    EXPECT_EQ(w.buffered(), 0u);
    EXPECT_EQ(s.writes, 1);
    EXPECT_EQ(value_of(w.write(std::string_view(block).substr(1))), block.size() - 1);
    EXPECT_EQ(w.available(), 1u);
    EXPECT_EQ(s.writes, 1);
    EXPECT_EQ(value_of(w.write("x")), 1u);   // fills the block: written out
    EXPECT_EQ(w.buffered(), 0u);
    EXPECT_EQ(w.available(), config::io_buffer_size);
    EXPECT_EQ(s.writes, 2);
    ASSERT_TRUE(w.write("tail"));
    EXPECT_TRUE(w.close());
    EXPECT_TRUE(w.is_closed());
    EXPECT_EQ(s.closes, 1);
    EXPECT_EQ(s.text.size(), 2 * block.size() + 4);
    EXPECT_TRUE(w.close());   // nothing kept: success again, nothing closed again
    EXPECT_EQ(s.closes, 1);
    EXPECT_EQ(error_of(w.write(slice<const byte>())).code(), errc::closed);   // even zero bytes after the close
    EXPECT_EQ(error_of(w.close()).code(), errc::closed);   // kept now
    EXPECT_EQ(error_of(w.flush()).code(), errc::closed);
}

// A writer whose stream fails part way: the first error kept, the close
// still closes the stream, nothing more is written
TEST(IoBufferedBounds_Tests, AWriterFailsPartWay) {
    sink s(10);
    buffered_writer w(s);
    std::string big(config::io_buffer_size + 5, 'z');
    EXPECT_EQ(error_of(w.write(big)).code(), std::errc::no_space_on_device);
    ASSERT_TRUE(w.last_error());
    int writes = s.writes;
    EXPECT_EQ(error_of(w.write("a")).code(), std::errc::no_space_on_device);
    EXPECT_EQ(s.writes, writes);
    EXPECT_EQ(error_of(w.close()).code(), std::errc::no_space_on_device);
    EXPECT_EQ(s.closes, 1);

    sink t(4);
    buffered_writer v(t);
    ASSERT_TRUE(v.write("abcdef"));   // buffered: no error yet
    EXPECT_EQ(error_of(v.flush()).code(), std::errc::no_space_on_device);
    EXPECT_EQ(v.buffered(), 6u);      // the bytes stay
    EXPECT_EQ(error_of(spawn(v.async_flush()).wait()).code(), std::errc::no_space_on_device);
    EXPECT_EQ(error_of(spawn(v.async_close()).wait()).code(), std::errc::no_space_on_device);
    EXPECT_EQ(t.closes, 1);
}

// A writer over a file closed under it: the write that reaches it is
// errc::closed, kept
TEST(IoBufferedBounds_Tests, AWriterOverAClosedFile) {
    auto p = pipe();
    ASSERT_TRUE(p);
    buffered_writer w(p->write);
    ASSERT_TRUE(p->write.close());
    ASSERT_TRUE(w.write("small"));   // into the block
    EXPECT_EQ(error_of(w.flush()).code(), errc::closed);
    EXPECT_EQ(error_of(w.close()).code(), errc::closed);
    ASSERT_TRUE(p->read.close());
}
