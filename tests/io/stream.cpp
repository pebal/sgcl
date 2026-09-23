//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <string_view>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    std::string_view as_text(std::span<const std::byte> b) {
        return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
    }

    // A reader that hands out its text in pieces of at most n bytes
    class dribble final : public reader {
    public:
        dribble(std::string_view s, size_t n) : _s(s), _n(n) {}

        result<size_t> read(slice<std::byte> out) override {
            size_t k = std::min({out.size(), _n, _s.size()});
            std::memcpy(out.data(), _s.data(), k);
            _s.remove_prefix(k);
            return k;
        }

        task<result<size_t>> async_read(slice<std::byte> out) override {
            co_return read(out);
        }

    private:
        std::string_view _s;
        size_t _n;
    };

    // A reader that fails after its text
    class failing final : public reader {
    public:
        explicit failing(std::string_view s) : _s(s) {}

        result<size_t> read(slice<std::byte> out) override {
            if (_s.empty()) {
                return sgcl::unexpected(error(std::make_error_code(std::errc::io_error), "read", "failing"));
            }
            size_t k = std::min(out.size(), _s.size());
            std::memcpy(out.data(), _s.data(), k);
            _s.remove_prefix(k);
            return k;
        }

        task<result<size_t>> async_read(slice<std::byte> out) override {
            co_return read(out);
        }

    private:
        std::string_view _s;
    };
}

TEST(IoStream_Tests, ErrorCarriesOpPathAndCode) {
    error e(std::make_error_code(std::errc::no_such_file_or_directory), "open", "log.txt");
    EXPECT_TRUE(e.is_not_found());
    EXPECT_FALSE(e.is_permission());
    EXPECT_EQ(e.op(), "open");
    EXPECT_EQ(e.path(), "log.txt");
    EXPECT_EQ(std::string_view(e.message()).substr(0, 14), "open log.txt: ");
    error eof(errc::unexpected_eof, "read");
    EXPECT_TRUE(eof.is_eof());
    EXPECT_EQ(eof.code().category().name(), std::string_view("io"));
    EXPECT_EQ(std::string_view(eof.message()), "read: unexpected end of stream");
    EXPECT_TRUE(error(errc::closed, "x").is_closed());
    EXPECT_TRUE(error(std::make_error_code(std::errc::operation_would_block), "x").is_timeout());
}

TEST(IoStream_Tests, BufferReadsWhatWasWritten) {
    sgcl::tracked_ptr b = make_tracked<buffer>();
    EXPECT_TRUE(b->empty());
    ASSERT_TRUE(b->write_text("hello "));
    ASSERT_TRUE(b->write_text("world"));
    EXPECT_EQ(b->size(), 11u);
    EXPECT_EQ(b->text(), "hello world");
    std::byte out[5];
    auto r = b->read(out);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 5u);
    EXPECT_EQ(as_text(std::span<const std::byte>(out, 5)), "hello");
    EXPECT_EQ(b->text(), " world");
    auto all = b->read_all_text();
    ASSERT_TRUE(all);
    EXPECT_EQ(std::string_view(*all), " world");
    EXPECT_TRUE(b->empty());
    r = b->read(out);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 0u);   // the end
    b->write_text("again");
    auto taken = b->release();
    EXPECT_EQ(as_text(std::span<const std::byte>(taken.data(), taken.size())), "again");
    EXPECT_TRUE(b->empty());
}

// write_text takes a string, a text slice (a piece of a string, a line of a
// buffered reader), a literal and a std::string_view, each written from
// where it lies
TEST(IoStream_Tests, WriteTextTakesAStringASliceALiteralAndAStdView) {
    sgcl::tracked_ptr b = make_tracked<buffer>();
    string s = "alpha beta";
    const char* p = "!";
    ASSERT_TRUE(b->write_text(s));
    ASSERT_TRUE(b->write_text(s.as_slice(6)));
    ASSERT_TRUE(b->write_text("x"));
    ASSERT_TRUE(b->write_text(p));
    std::string std_text = "?";
    ASSERT_TRUE(b->write_text(std_text));                 // a std::string, through its view
    ASSERT_TRUE(b->write_text(std::string_view(std_text).substr(0, 0)));
    EXPECT_EQ(b->text(), "alpha betabetax!?");
    auto t = sgcl::spawn([b, s]() -> task<size_t> {
        size_t total = *co_await b->async_write_text(s.as_slice(0, 5));
        total += *co_await b->async_write_text("y");
        co_return total;
    });
    EXPECT_EQ(t.join(), 6u);
    EXPECT_EQ(b->text(), "alpha betabetax!?alphay");
    sgcl::scheduler::stop();
}

TEST(IoStream_Tests, ReadFullAndUnexpectedEof) {
    sgcl::tracked_ptr d = make_tracked<dribble>("abcdefgh", 3);
    std::byte out[6];
    auto r = d->read_full(out);
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 6u);
    EXPECT_EQ(as_text(std::span<const std::byte>(out, 6)), "abcdef");
    r = d->read_full(out);   // two bytes left, six asked
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_eof());
    r = d->read_full(out);   // nothing left: the end, not an error
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 0u);
}

TEST(IoStream_Tests, ReadAllGrowsPastTheFirstBlock) {
    std::string big(3 * sgcl::config::IoBufferSize + 17, 'x');
    sgcl::tracked_ptr d = make_tracked<dribble>(big, 1000);
    auto r = d->read_all();
    ASSERT_TRUE(r);
    EXPECT_EQ(r->size(), big.size());
    EXPECT_TRUE(std::all_of(r->begin(), r->end(), [](std::byte b) { return b == std::byte('x'); }));
    sgcl::tracked_ptr f = make_tracked<failing>("partial");
    auto e = f->read_all();
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), std::make_error_code(std::errc::io_error));
}

TEST(IoStream_Tests, CopyMovesEverything) {
    std::string big(2 * sgcl::config::IoCopyBufferSize + 5, 'y');
    sgcl::tracked_ptr src = make_tracked<dribble>(big, 7000);
    sgcl::tracked_ptr dst = make_tracked<buffer>();
    auto n = copy(*dst, *src);
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, big.size());
    EXPECT_EQ(dst->size(), big.size());
    sgcl::tracked_ptr src2 = make_tracked<buffer>(std::string_view("abc"));
    sgcl::tracked_ptr dst2 = make_tracked<buffer>();
    EXPECT_EQ(*dst2->copy_from(*src2), 3u);
    EXPECT_EQ(dst2->text(), "abc");
    EXPECT_EQ(*src2->copy_to(*discard()), 0u);
}

TEST(IoStream_Tests, LimitTeeMulti) {
    sgcl::tracked_ptr src = make_tracked<buffer>(std::string_view("0123456789"));
    sgcl::tracked_ptr lim = make_tracked<limit_reader>(src, 4);
    auto r = lim->read_all_text();
    ASSERT_TRUE(r);
    EXPECT_EQ(std::string_view(*r), "0123");
    EXPECT_EQ(lim->remaining(), 0u);
    EXPECT_EQ(src->text(), "456789");

    sgcl::tracked_ptr seen = make_tracked<buffer>();
    sgcl::tracked_ptr tee = make_tracked<tee_reader>(src, seen);
    EXPECT_EQ(std::string_view(*tee->read_all_text()), "456789");
    EXPECT_EQ(seen->text(), "456789");

    sgcl::vector<tracked_ptr<reader>> parts;
    parts.push_back(make_tracked<buffer>(std::string_view("ab")));
    parts.push_back(make_tracked<buffer>(std::string_view("")));
    parts.push_back(make_tracked<dribble>("cde", 1));
    sgcl::tracked_ptr multi = make_tracked<multi_reader>(std::move(parts));
    EXPECT_EQ(std::string_view(*multi->read_all_text()), "abcde");

    sgcl::tracked_ptr w1 = make_tracked<buffer>(), w2 = make_tracked<buffer>();
    sgcl::vector<tracked_ptr<writer>> ws;
    ws.push_back(w1);
    ws.push_back(w2);
    sgcl::tracked_ptr mw = make_tracked<multi_writer>(std::move(ws));
    EXPECT_EQ(*mw->write_text("both"), 4u);
    EXPECT_EQ(w1->text(), "both");
    EXPECT_EQ(w2->text(), "both");
}

TEST(IoStream_Tests, AsyncFormsOnTheScheduler) {
    auto t = sgcl::spawn([]() -> task<std::string> {
        sgcl::tracked_ptr src = make_tracked<dribble>("async copy of some text", 5);
        sgcl::tracked_ptr dst = make_tracked<buffer>();
        auto n = co_await async_copy(*dst, *src);
        if (!n || *n != 23) {
            co_return "copy failed";
        }
        auto all = co_await dst->async_read_all_text();
        if (!all) {
            co_return "read failed";
        }
        std::byte out[4];
        sgcl::tracked_ptr d2 = make_tracked<dribble>("xy", 1);
        auto full = co_await d2->async_read_full(out);
        if (full || !full.error().is_eof()) {
            co_return "read_full should fail";
        }
        co_return all->str();
    }());
    EXPECT_EQ(t.join(), "async copy of some text");
    sgcl::scheduler::stop();
}
