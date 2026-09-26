//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

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
}

TEST(IoBuffered_Tests, LinesWithAndWithoutTerminators) {
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(make_tracked<dribble>("one\ntwo\r\n\nlast", 4));
    auto l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "one");
    l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "two");   // the \r stripped
    l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "");
    l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "last");   // no terminator: a line still
    l = r->read_line();
    ASSERT_TRUE(l);
    EXPECT_FALSE(*l);         // the end
    l = r->read_line();
    ASSERT_TRUE(l);
    EXPECT_FALSE(*l);         // and stays the end
}

TEST(IoBuffered_Tests, ReadsInBlocksNotBytes) {
    std::string text;
    for (int i = 0; i < 1000; ++i) {
        text += "line " + std::to_string(i) + "\n";
    }
    sgcl::tracked_ptr src = make_tracked<dribble>(text, 1 << 20);
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(src);
    int n = 0;
    for (auto line : r->lines()) {
        EXPECT_EQ(line, "line " + std::to_string(n));
        ++n;
    }
    EXPECT_EQ(n, 1000);
    EXPECT_FALSE(r->last_error());
    EXPECT_LE(src->reads, 3);   // ~9 KB of text: two blocks and the read that sees the end
}

TEST(IoBuffered_Tests, ALineLongerThanTheBlock) {
    std::string longline(3 * sgcl::config::io_buffer_size + 100, 'L');
    std::string text = "short\n" + longline + "\nafter\n";
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(make_tracked<dribble>(text, 5000));
    auto l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "short");
    l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ((*l)->size(), longline.size());
    EXPECT_EQ(**l, longline);
    l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "after");
    // a long last line without a terminator
    sgcl::tracked_ptr r2 = make_tracked<buffered_reader>(make_tracked<dribble>(longline, 3000));
    l = r2->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, longline);
}

TEST(IoBuffered_Tests, MaxLineBounds) {
    std::string text = "ok\n" + std::string(100, 'x') + "\nnext\n";
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(make_tracked<dribble>(text, 1 << 20));
    r->set_max_line(50);
    auto l = r->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ(**l, "ok");
    l = r->read_line();
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code(), make_error_code(errc::line_too_long));
    // a bound longer than the block, and a line between the two
    std::string mid(sgcl::config::io_buffer_size + 10, 'm');
    sgcl::tracked_ptr r2 = make_tracked<buffered_reader>(make_tracked<dribble>(mid + "\n" + mid + mid + "\n", 4000));
    r2->set_max_line(2 * sgcl::config::io_buffer_size);
    l = r2->read_line();
    ASSERT_TRUE(l && *l);
    EXPECT_EQ((*l)->size(), mid.size());
    l = r2->read_line();
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code(), make_error_code(errc::line_too_long));
}

TEST(IoBuffered_Tests, ReadUntilPeekByteDiscard) {
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(make_tracked<dribble>("a,bb,,ccc", 2));
    auto t = r->read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, "a,");                                   // with its delimiter, as Go's ReadString
    auto p = r->peek(3);
    ASSERT_TRUE(p);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(p->data()), p->size()), "bb,");
    EXPECT_GE(r->buffered(), 3u);
    t = r->read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, "bb,");
    t = r->read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, ",");
    auto b = r->read_byte();
    ASSERT_TRUE(b && *b);
    EXPECT_EQ(**b, byte('c'));
    EXPECT_EQ(*r->discard(1), 1u);
    t = r->read_until(',');
    ASSERT_TRUE(t && *t);
    EXPECT_EQ(**t, "c");                                    // the last token, with no delimiter after it
    EXPECT_EQ(*r->discard(5), 0u);
    b = r->read_byte();
    ASSERT_TRUE(b);
    EXPECT_FALSE(*b);
}

TEST(IoBuffered_Tests, ReadMixedWithLines) {
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(make_tracked<dribble>("head\n" + std::string(20000, 'b') + "tail", 1 << 20));
    EXPECT_EQ(**r->read_line(), "head");
    byte small[10];
    EXPECT_EQ(*r->read(small), 10u);            // from the block
    std::string rest(19990, '\0');
    EXPECT_EQ(*r->read_full(std::as_writable_bytes(std::span(rest))), 19990u);   // larger than the block: through the block, then direct
    EXPECT_EQ(rest, std::string(19990, 'b'));
    EXPECT_EQ(**r->read_line(), "tail");
}

TEST(IoBuffered_Tests, WriterBuffersAndFlushes) {
    sgcl::tracked_ptr sink = make_tracked<counting>();
    sgcl::tracked_ptr w = make_tracked<buffered_writer>(sink);
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(w->write("0123456789"));
    }
    EXPECT_EQ(sink->writes, 0);   // 1000 bytes: all in the block
    EXPECT_EQ(w->buffered(), 1000u);
    ASSERT_TRUE(w->flush());
    EXPECT_EQ(sink->writes, 1);
    EXPECT_EQ(sink->text.size(), 1000u);
    // more than the block in one write: the block first, then direct
    std::string big(3 * sgcl::config::io_buffer_size, 'z');
    ASSERT_TRUE(w->write("abc"));
    ASSERT_TRUE(w->write(string(big)));
    EXPECT_EQ(w->buffered(), 0u);
    EXPECT_EQ(sink->text.size(), 1003 + big.size());
    EXPECT_EQ(sink->text.substr(1000, 3), "abc");
    // exactly filling the block flushes it
    std::string fill(sgcl::config::io_buffer_size, 'f');
    ASSERT_TRUE(w->write(string(fill)));
    EXPECT_EQ(w->buffered(), 0u);
    ASSERT_TRUE(w->close());
    EXPECT_TRUE(w->is_closed());
    auto after = w->write("x");
    ASSERT_FALSE(after);
    EXPECT_TRUE(after.error().is_closed());
}

TEST(IoBuffered_Tests, AsyncLines) {
    auto t = sgcl::async::spawn([]() -> task<int> {
        std::string text;
        for (int i = 0; i < 300; ++i) {
            text += std::to_string(i) + "\n";
        }
        sgcl::tracked_ptr r = make_tracked<buffered_reader>(make_tracked<dribble>(text, 77));
        int n = 0;
        for (;;) {
            auto l = co_await r->async_read_line();
            if (!l || !*l) {
                break;
            }
            if (**l != std::to_string(n)) {
                co_return -1;
            }
            ++n;
        }
        sgcl::tracked_ptr sink = make_tracked<counting>();
        sgcl::tracked_ptr w = make_tracked<buffered_writer>(sink);
        co_await w->async_write("async");
        co_await w->async_flush();
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
    sgcl::tracked_ptr r = make_tracked<buffered_reader>(make_tracked<dribble>(text, 1 << 20));
    auto first = r->read_line();
    ASSERT_TRUE(first && *first);
    slice<const char> kept = **first;
    EXPECT_TRUE(kept.owned());
    EXPECT_EQ(kept, "line 0");
    for (int i = 1; i < 3000; ++i) {                             // the block refilled many times over
        auto l = r->read_line();
        ASSERT_TRUE(l && *l);
    }
    collector::force_collect(true);
    EXPECT_EQ(kept.size(), 6u);                                 // the slice's memory is alive: the block it holds
    string copy(kept);                                          // a string of a slice that is not a string's: a copy
    EXPECT_EQ(copy.size(), 6u);
    std::string own(kept.begin(), kept.end());
    EXPECT_TRUE(own.rfind("line ", 0) == 0);                    // the block is reused for later lines: a kept line is copied when its text matters
    sgcl::tracked_ptr r2 = make_tracked<buffered_reader>(make_tracked<dribble>("short\n" + std::string(3 * sgcl::config::io_buffer_size, 'L') + "\n", 5000));
    auto s0 = r2->read_line();
    auto s1 = r2->read_line();                                  // the long line: a slice of the reader's vector
    ASSERT_TRUE(s1 && *s1);
    EXPECT_TRUE((*s1)->owned());
    EXPECT_NE((*s1)->owner(), (*s0)->owner());                  // another object than the block
    EXPECT_EQ((*s1)->size(), 3u * sgcl::config::io_buffer_size);
}

