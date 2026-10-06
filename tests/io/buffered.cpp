//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <ranges>

using namespace sgcl::async;

#include <string>
#include <string_view>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    // A reader that hands out its text in pieces of at most n bytes
    class dribble final : public io::mixin::reader<dribble> {
    public:
        dribble(std::string s, size_t n) : _s(std::move(s)), _n(n) {}

        expected<size_t, io::error> read(slice<byte> out) {
            size_t k = std::min({out.size(), _n, _s.size() - _pos});
            std::memcpy(out.data(), _s.data() + _pos, k);
            _pos += k;
            ++reads;
            return k;
        }

        task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

        int reads = 0;

    private:
        std::string _s;
        size_t _pos = 0;
        size_t _n;
    };

    // A writer that counts its calls
    class counting final : public io::mixin::writer<counting> {
    public:
        using io::mixin::writer<counting>::write;
        using io::mixin::writer<counting>::async_write;

        expected<size_t, io::error> write(slice<const byte> data) {
            text.append(reinterpret_cast<const char*>(data.data()), data.size());
            ++writes;
            return data.size();
        }

        task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            co_return write(data);
        }

        std::string text;
        int writes = 0;
    };

    // A writer that takes `room` bytes and then fails: EIO the first time,
    // ENOSPC every time after, so that a failure handed on is told from a
    // new one; `calls` counts the writes tried, `closes` the closes
    class failing_after final : public io::mixin::writer<failing_after> {
    public:
        using io::mixin::writer<failing_after>::write;
        using io::mixin::writer<failing_after>::async_write;

        explicit failing_after(size_t room) : _room(room) {}

        expected<size_t, io::error> write(slice<const byte> data) {
            ++calls;
            if (failures || text.size() + data.size() > _room) {
                ++failures;
                return sgcl::unexpected(io::error(sgcl::error_code(failures == 1 ? EIO : ENOSPC, std::system_category()), "write", "disk"));
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

        task<expected<void, io::error>> async_close() {
            co_return close();
        }

        std::string text;
        int calls = 0;
        int failures = 0;
        int closes = 0;

    private:
        size_t _room;
    };

    bool is_eio(const io::error& e) {
        return e.code() == sgcl::error_code(EIO, std::system_category());
    }
}

TEST(IoBuffered_Tests, LinesWithAndWithoutTerminators) {
    buffered_reader r(make_tracked<dribble>("one\ntwo\r\n\nlast", 4));
    auto l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "one");
    l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "two");   // the \r stripped
    l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "");
    l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "last");   // no terminator: a line still
    l = r.read_line();
    ASSERT_TRUE(l);
    EXPECT_FALSE(*l);         // the end
    l = r.read_line();
    ASSERT_TRUE(l);
    EXPECT_FALSE(*l);         // and stays the end
}

// lines() reads a line at the look at it, and ++ reads nothing: take(n)
// reads n lines, and the reader goes on from the next one
TEST(IoBuffered_Tests, TakeOfLinesLeavesTheRestToTheReader) {
    sgcl::tracked_ptr src = make_tracked<dribble>(std::string("one\ntwo\nthree\n"), 1 << 20);
    buffered_reader r(src);
    int taken = 0;
    for (auto line : r.lines() | std::views::take(1)) {
        EXPECT_EQ(line, "one");
        ++taken;
    }
    EXPECT_EQ(taken, 1);
    auto l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "two");
}

TEST(IoBuffered_Tests, ReadsInBlocksNotBytes) {
    std::string text;
    for (int i = 0; i < 1000; ++i) {
        text += "line " + std::to_string(i) + "\n";
    }
    sgcl::tracked_ptr src = make_tracked<dribble>(text, 1 << 20);
    buffered_reader r(src);
    int n = 0;
    for (auto line : r.lines()) {
        EXPECT_EQ(line, "line " + std::to_string(n));
        ++n;
    }
    EXPECT_EQ(n, 1000);
    EXPECT_FALSE(r.last_error());
    EXPECT_LE(src->reads, 3);   // ~9 KB of text: two blocks and the read that sees the end
}

TEST(IoBuffered_Tests, ALineLongerThanTheBlock) {
    std::string longline(3 * sgcl::config::io_buffer_size + 100, 'L');
    std::string text = "short\n" + longline + "\nafter\n";
    buffered_reader r(make_tracked<dribble>(text, 5000));
    auto l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "short");
    l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ((*l)->size(), longline.size());
    EXPECT_EQ(**l, longline);
    l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "after");
    // a long last line without a terminator
    buffered_reader r2(make_tracked<dribble>(longline, 3000));
    l = r2.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, longline);
}

TEST(IoBuffered_Tests, MaxLineBounds) {
    std::string text = "ok\n" + std::string(100, 'x') + "\nnext\n";
    buffered_reader r(make_tracked<dribble>(text, 1 << 20));
    r.set_max_line(50);
    auto l = r.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "ok");
    l = r.read_line();
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code(), make_error_code(errc::line_too_long));
    // a bound longer than the block, and a line between the two
    std::string mid(sgcl::config::io_buffer_size + 10, 'm');
    buffered_reader r2(make_tracked<dribble>(mid + "\n" + mid + mid + "\n", 4000));
    r2.set_max_line(2 * sgcl::config::io_buffer_size);
    l = r2.read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ((*l)->size(), mid.size());
    l = r2.read_line();
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code(), make_error_code(errc::line_too_long));
}

// A line past the bound is skipped whole, wherever its end is: in what is
// buffered, past it (the reader reads on, dropping the bytes, to the
// delimiter), past the block, or nowhere (the end of the stream). The
// next read starts after it; before, a line whose end was not buffered
// gave line_too_long again at every read
TEST(IoBuffered_Tests, ALineTooLongIsSkipped) {
    auto too_long = [](const expected<optional<slice<const char>>, io::error>& l) {
        return !l && l.error().code() == make_error_code(errc::line_too_long);
    };
    std::string text = "ok\n" + std::string(100, 'x') + "\nnext\n" + std::string(100, 'y');
    buffered_reader r(make_tracked<dribble>(text, 20));   // 20 bytes a read: the end of the long line not buffered yet
    r.set_max_line(50);
    EXPECT_EQ(value_of(r.read_line()).value(), "ok");
    EXPECT_TRUE(too_long(r.read_line()));
    EXPECT_EQ(value_of(r.read_line()).value(), "next");
    EXPECT_TRUE(too_long(r.read_line()));   // the last line, too long, without its end
    EXPECT_FALSE(value_of(r.read_line()));   // then the end of the stream

    // past the block, read_until's delimiter, a task's read
    std::string mid(sgcl::config::io_buffer_size + 10, 'm');
    buffered_reader r2(make_tracked<dribble>(mid + mid + mid + ";after;", 4000));
    r2.set_max_line(2 * sgcl::config::io_buffer_size);
    auto t = sgcl::async::spawn([r2, too_long]() -> task<std::string> {
        if (!too_long(co_await r2.async_read_until(';'))) {
            co_return "not too long";
        }
        auto next = co_await r2.async_read_until(';');
        co_return next && *next ? std::string(**next) : "no next token";
    });
    EXPECT_EQ(t.wait(), "after;");
    sgcl::async::scheduler::stop();
}

TEST(IoBuffered_Tests, ReadUntilPeekByteDiscard) {
    buffered_reader r(make_tracked<dribble>("a,bb,,ccc", 2));
    auto t = r.read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, "a,");                                   // with its delimiter, as Go's ReadString
    auto p = r.peek(3);
    ASSERT_TRUE(p);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(p->data()), p->size()), "bb,");
    EXPECT_GE(r.buffered(), 3u);
    t = r.read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, "bb,");
    t = r.read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, ",");
    auto b = r.read_byte();
    ASSERT_TRUE(b && *b);
    EXPECT_EQ(**b, byte('c'));
    EXPECT_EQ(value_of(r.discard(1)), 1u);
    t = r.read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, "c");                                    // the last token, with no delimiter after it
    EXPECT_EQ(value_of(r.discard(5)), 0u);
    b = r.read_byte();
    ASSERT_TRUE(b);
    EXPECT_FALSE(*b);
}

TEST(IoBuffered_Tests, ReadMixedWithLines) {
    buffered_reader r(make_tracked<dribble>("head\n" + std::string(20000, 'b') + "tail", 1 << 20));
    EXPECT_EQ(*value_of(r.read_line()), "head");
    byte small[10];
    EXPECT_EQ(value_of(r.read(small)), 10u);            // from the block
    std::string rest(19990, '\0');
    EXPECT_EQ(value_of(r.read_full(std::as_writable_bytes(std::span(rest)))), 19990u);   // larger than the block: through the block, then direct
    EXPECT_EQ(rest, std::string(19990, 'b'));
    EXPECT_EQ(*value_of(r.read_line()), "tail");
}

TEST(IoBuffered_Tests, WriterBuffersAndFlushes) {
    sgcl::tracked_ptr sink = make_tracked<counting>();
    buffered_writer w(sink);
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(w.write("0123456789"));
    }
    EXPECT_EQ(sink->writes, 0);   // 1000 bytes: all in the block
    EXPECT_EQ(w.buffered(), 1000u);
    ASSERT_TRUE(w.flush());
    EXPECT_EQ(sink->writes, 1);
    EXPECT_EQ(sink->text.size(), 1000u);
    // more than the block in one write: the block first, then direct
    std::string big(3 * sgcl::config::io_buffer_size, 'z');
    ASSERT_TRUE(w.write("abc"));
    ASSERT_TRUE(w.write(string(big)));
    EXPECT_EQ(w.buffered(), 0u);
    EXPECT_EQ(sink->text.size(), 1003 + big.size());
    EXPECT_EQ(sink->text.substr(1000, 3), "abc");
    // exactly filling the block flushes it
    std::string fill(sgcl::config::io_buffer_size, 'f');
    ASSERT_TRUE(w.write(string(fill)));
    EXPECT_EQ(w.buffered(), 0u);
    ASSERT_TRUE(w.close());
    EXPECT_TRUE(w.is_closed());
    auto after = w.write("x");
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
}

// The first failure of the writer underneath is kept: every write and
// flush after it gives that failure at once and writes nothing, close()
// gives it (and still closes w), a second close too; last_error() holds it
TEST(IoBuffered_Tests, AWriterKeepsItsFirstFailure) {
    // the block's flush fails
    sgcl::tracked_ptr sink = make_tracked<failing_after>(4);
    buffered_writer w(sink);
    ASSERT_TRUE(w.write("0123456789"));   // in the block
    EXPECT_FALSE(w.last_error());
    auto flushed = w.flush();
    ASSERT_FALSE(flushed);
    EXPECT_TRUE(is_eio(flushed.error()));
    EXPECT_EQ(sink->calls, 1);
    auto more = w.write("x");
    ASSERT_FALSE(more);
    EXPECT_TRUE(is_eio(more.error()));
    auto big = w.write(string(std::string(2 * sgcl::config::io_buffer_size, 'z')));
    ASSERT_FALSE(big);
    EXPECT_TRUE(is_eio(big.error()));
    auto again = w.flush();
    ASSERT_FALSE(again);
    EXPECT_TRUE(is_eio(again.error()));
    auto closed = w.close();
    ASSERT_FALSE(closed);
    EXPECT_TRUE(is_eio(closed.error()));
    EXPECT_EQ(sink->closes, 1);
    auto second = w.close();
    ASSERT_FALSE(second);
    EXPECT_TRUE(is_eio(second.error()));
    EXPECT_EQ(sink->closes, 1);
    EXPECT_EQ(sink->calls, 1);   // nothing tried after the failure
    ASSERT_TRUE(w.last_error());
    EXPECT_TRUE(is_eio(*w.last_error()));

    // a write past the block, straight to w, fails; the task's forms
    auto t = sgcl::async::spawn([]() -> task<std::string> {
        sgcl::tracked_ptr sink = make_tracked<failing_after>(100);
        buffered_writer w(sink);
        auto big = co_await w.async_write(string(std::string(sgcl::config::io_buffer_size, 'z')));
        if (big || !is_eio(big.error())) co_return "big";
        auto more = co_await w.async_write("x");
        if (more || !is_eio(more.error())) co_return "write after";
        auto flushed = co_await w.async_flush();
        if (flushed || !is_eio(flushed.error())) co_return "flush after";
        auto closed = co_await w.async_close();
        if (closed || !is_eio(closed.error())) co_return "close";
        if (sink->calls != 1 || sink->closes != 1 || sink->text.size() != 0) co_return "written after";
        co_return "ok";
    }());
    EXPECT_EQ(t.wait(), "ok");
    sgcl::async::scheduler::stop();
}

// A write after close is the caller's error, kept as a failure of w is:
// a flush and every close after it give it, w closed once
TEST(IoBuffered_Tests, AWriteAfterCloseIsKept) {
    sgcl::tracked_ptr sink = make_tracked<failing_after>(100);
    buffered_writer w(sink);
    ASSERT_TRUE(w.write("abc"));
    ASSERT_TRUE(w.close());
    ASSERT_TRUE(w.close());   // a second close does nothing
    EXPECT_FALSE(w.last_error());
    auto late = w.write("x");
    ASSERT_FALSE(late);
    EXPECT_TRUE(late.error().is_closed());
    auto more = w.write("y");
    ASSERT_FALSE(more);
    EXPECT_TRUE(more.error() == late.error());
    auto flushed = w.flush();
    ASSERT_FALSE(flushed);
    EXPECT_TRUE(flushed.error() == late.error());
    auto closed = w.close();
    ASSERT_FALSE(closed);
    EXPECT_TRUE(closed.error() == late.error());
    ASSERT_TRUE(w.last_error());
    EXPECT_TRUE(*w.last_error() == late.error());
    EXPECT_EQ(sink->text, "abc");
    EXPECT_EQ(sink->closes, 1);
}

TEST(IoBuffered_Tests, AsyncLines) {
    auto t = sgcl::async::spawn([]() -> task<int> {
        std::string text;
        for (int i = 0; i < 300; ++i) {
            text += std::to_string(i) + "\n";
        }
        buffered_reader r(make_tracked<dribble>(text, 77));
        int n = 0;
        for (;;) {
            auto l = co_await r.async_read_line();
            if (!l || !*l) {
                break;
            }
            if (**l != std::to_string(n)) {
                co_return -1;
            }
            ++n;
        }
        sgcl::tracked_ptr sink = make_tracked<counting>();
        buffered_writer w(sink);
        co_await w.async_write("async");
        co_await w.async_flush();
        if (sink->text != "async") {
            co_return -2;
        }
        co_return n;
    }());
    EXPECT_EQ(t.wait(), 300);
    sgcl::async::scheduler::stop();
}

TEST(IoBuffered_Tests, ALineIsASliceThatHoldsTheBlock) {
    // A line is a slice of the reader's block: kept past the next reads
    // it still points into that block, alive as long as the slice is —
    // its characters those of the moment it was made, not reused memory
    std::string text;
    for (int i = 0; i < 3000; ++i) {
        text += "line " + std::to_string(i) + "\n";           // several blocks' worth
    }
    buffered_reader r(make_tracked<dribble>(text, 1 << 20));
    auto first = r.read_line();
    ASSERT_TRUE(first && *first);
    slice<const char> kept = **first;
    EXPECT_TRUE(kept.owned());
    EXPECT_EQ(kept, "line 0");
    for (int i = 1; i < 3000; ++i) {                             // the block refilled many times over
        auto l = r.read_line();
        ASSERT_TRUE(l && *l);
    }
    collector::force_collect(true);
    EXPECT_EQ(kept.size(), 6u);                                 // the slice's memory is alive: the block it holds
    string copy(kept);                                          // a string of a slice that is not a string's: a copy
    EXPECT_EQ(copy.size(), 6u);
    std::string own(kept.begin(), kept.end());
    EXPECT_TRUE(own.rfind("line ", 0) == 0);                    // the block is reused for later lines: a kept line is copied when its text matters
    buffered_reader r2(make_tracked<dribble>("short\n" + std::string(3 * sgcl::config::io_buffer_size, 'L') + "\n", 5000));
    auto s0 = r2.read_line();
    auto s1 = r2.read_line();                                  // the long line: a slice of the reader's vector
    ASSERT_TRUE(s1 && *s1);
    EXPECT_TRUE((*s1)->owned());
    ASSERT_TRUE(s0 && *s0);
    EXPECT_NE((*s1)->owner(), (*s0)->owner());                  // another object than the block
    EXPECT_EQ((*s1)->size(), 3u * sgcl::config::io_buffer_size);
}

// The buffered streams are handles: copies share one reader (one block,
// one position) or one writer (one block, one kept error); a stream made
// of a temporary handle keeps its state; an empty handle is an empty stream
TEST(IoBuffered_Tests, TheBufferedStreamsAreHandles) {
    static_assert(sizeof(buffered_reader) == sizeof(sgcl::tracked_ptr<void>));
    static_assert(sizeof(buffered_writer) == sizeof(sgcl::tracked_ptr<void>));
    buffered_reader r(make_tracked<dribble>("one\ntwo\nthree\n", 3));
    buffered_reader copy = r;
    EXPECT_TRUE(copy == r);
    EXPECT_EQ(*value_of(r.read_line()), "one");
    EXPECT_EQ(*value_of(copy.read_line()), "two");                       // the same position
    EXPECT_EQ(r.buffered(), copy.buffered());

    io::reader through(buffered_reader(make_tracked<dribble>("kept\n", 2)));
    collector::force_collect(true);
    EXPECT_EQ(std::string_view(value_of(through.read_all_text())), "kept\n");

    sgcl::tracked_ptr sink = make_tracked<counting>();
    buffered_writer w(sink);
    buffered_writer other = w;
    ASSERT_TRUE(w.write("ab"));
    ASSERT_TRUE(other.write("cd"));
    EXPECT_EQ(w.buffered(), 4u);                                // one block
    ASSERT_TRUE(other.flush());
    EXPECT_EQ(sink->text, "abcd");
    io::writer out{buffered_writer(sink)};
    collector::force_collect(true);
    ASSERT_TRUE(out.write(std::string("ef")));
    ASSERT_TRUE(out.close());                                   // flushed by its close
    EXPECT_EQ(sink->text, "abcdef");

    buffered_reader none;
    EXPECT_FALSE(none);
    EXPECT_FALSE(io::reader(none));
    EXPECT_FALSE(io::writer(buffered_writer()));
}

