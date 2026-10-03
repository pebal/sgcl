//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The CSV, JSON and XML writers after their stream failed (DESIGN 408): the
// failure is kept, every later flush gives it, and what the program goes
// on writing is dropped, not gathered for a flush that never writes —
// memory a long-running writer would hold without bound. The text is
// plain memory: measured by what the system's allocator holds (as
// tests/net/http/heap.cpp measures its wire).
#include "common.h"

#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif

using namespace enc_test;
using sgcl::encoding::csv;
using sgcl::encoding::errc;
using sgcl::encoding::json;
using sgcl::encoding::xml;
using sgcl::string;

namespace {
    size_t malloc_in_use() {
#if defined(__APPLE__)
        malloc_statistics_t st;
        malloc_zone_statistics(nullptr, &st);
        return st.size_in_use;
#else
        return 0;
#endif
    }

    // 2000 records of 4 KB: 8 MB a writer that gathers would hold
    constexpr int Records = 2000;
    const std::string& field() {
        static const std::string f(4096, 'x');
        return f;
    }

    constexpr size_t Allowed = size_t(1) << 20;
}

TEST(EncodingWriters_Tests, CsvDropsWhatComesAfterItsStreamFailed) {
#if !defined(__APPLE__)
    GTEST_SKIP() << "the allocator's statistics are read on macOS";
#endif
    sgcl::tracked_ptr out = make_tracked<flaky>();
    csv::writer w(out);
    w.write({"a", "b"});
    out->fail_next = 1;
    auto f = w.flush();
    ASSERT_FALSE(f);
    size_t before = malloc_in_use();
    string text(field());
    for (int i = 0; i < Records; ++i) {
        w.write({text, text});
    }
    size_t after = malloc_in_use();
    EXPECT_LT(after - std::min(after, before), Allowed);
    auto again = w.flush();
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().path(), "flaky");
    EXPECT_EQ(out->text, "");
    // in a task, the same
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<size_t> {
        sgcl::tracked_ptr out = make_tracked<flaky>();
        csv::writer w(out);
        w.write({"a"});
        out->fail_next = 1;
        if (co_await w.async_flush()) {
            co_return SIZE_MAX;
        }
        size_t before = malloc_in_use();
        string text(field());
        for (int i = 0; i < Records; ++i) {
            w.write({text});
        }
        size_t after = malloc_in_use();
        if (co_await w.async_flush() || !out->text.empty()) {
            co_return SIZE_MAX;
        }
        co_return after - std::min(after, before);
    }());
    EXPECT_LT(t.wait(), Allowed);
    sgcl::async::scheduler::stop();
}

TEST(EncodingWriters_Tests, JsonDropsWhatComesAfterItsStreamFailed) {
#if !defined(__APPLE__)
    GTEST_SKIP() << "the allocator's statistics are read on macOS";
#endif
    sgcl::tracked_ptr out = make_tracked<flaky>();
    json::writer w(out);
    w.value(1);
    out->fail_next = 1;
    auto f = w.flush();
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().path(), "flaky");
    size_t before = malloc_in_use();
    string text(field());
    for (int i = 0; i < Records; ++i) {
        w.begin_object().key("k").value(text).end_object();
    }
    size_t after = malloc_in_use();
    EXPECT_LT(after - std::min(after, before), Allowed);
    auto again = w.flush();
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().path(), "flaky");   // the stream's failure, not a mistake of the structure
    EXPECT_EQ(again.error().code(), std::make_error_code(std::errc::timed_out));
    EXPECT_EQ(out->text, "");
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<size_t> {
        sgcl::tracked_ptr out = make_tracked<flaky>();
        json::writer w(out);
        w.value(1);
        out->fail_next = 1;
        if (co_await w.async_flush()) {
            co_return SIZE_MAX;
        }
        size_t before = malloc_in_use();
        string text(field());
        for (int i = 0; i < Records; ++i) {
            w.value(text);
        }
        size_t after = malloc_in_use();
        auto again = co_await w.async_flush();
        if (again || again.error().path() != "flaky" || !out->text.empty()) {
            co_return SIZE_MAX;
        }
        co_return after - std::min(after, before);
    }());
    EXPECT_LT(t.wait(), Allowed);
    sgcl::async::scheduler::stop();
}

TEST(EncodingWriters_Tests, XmlDropsWhatComesAfterItsStreamFailed) {
#if !defined(__APPLE__)
    GTEST_SKIP() << "the allocator's statistics are read on macOS";
#endif
    sgcl::tracked_ptr out = make_tracked<flaky>();
    xml::writer w(out);
    w.start("log");
    out->fail_next = 1;
    auto f = w.flush();
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().path(), "flaky");
    EXPECT_FALSE(w.last_error());   // no mistake of the document's (XmlWriter_Tests.AFailingStream)
    size_t before = malloc_in_use();
    string text(field());
    for (int i = 0; i < Records; ++i) {
        w.start("entry").attribute("v", text).text(text).end();
    }
    size_t after = malloc_in_use();
    EXPECT_LT(after - std::min(after, before), Allowed);
    auto again = w.flush();
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().path(), "flaky");
    EXPECT_EQ(again.error().code(), std::make_error_code(std::errc::timed_out));
    EXPECT_EQ(out->text, "");
    EXPECT_FALSE(w.last_error());
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<size_t> {
        sgcl::tracked_ptr out = make_tracked<flaky>();
        xml::writer w(out);
        w.start("log");
        out->fail_next = 1;
        if (co_await w.async_flush()) {
            co_return SIZE_MAX;
        }
        size_t before = malloc_in_use();
        string text(field());
        for (int i = 0; i < Records; ++i) {
            w.start("e").text(text).end();
        }
        size_t after = malloc_in_use();
        auto again = co_await w.async_flush();
        if (again || again.error().path() != "flaky" || !out->text.empty()) {
            co_return SIZE_MAX;
        }
        co_return after - std::min(after, before);
    }());
    EXPECT_LT(t.wait(), Allowed);
    sgcl::async::scheduler::stop();
}

// The success path is the same: a writer whose stream works writes all
TEST(EncodingWriters_Tests, AWorkingStreamGetsEverything) {
    sgcl::tracked_ptr c = make_tracked<sink>();
    csv::writer cw(c);
    sgcl::tracked_ptr j = make_tracked<sink>();
    json::writer jw(j);
    sgcl::tracked_ptr x = make_tracked<sink>();
    xml::writer xw(x);
    for (int i = 0; i < 3; ++i) {
        cw.write({"a", "b"});
        ASSERT_TRUE(cw.flush());
        jw.value(i);
        ASSERT_TRUE(jw.flush());
        xw.start("e").end();
        ASSERT_TRUE(xw.flush());
    }
    EXPECT_EQ(c->text, "a,b\na,b\na,b\n");
    EXPECT_EQ(j->text, "0\n1\n2\n");
    EXPECT_EQ(x->text, "<e/><e/><e/>");
    EXPECT_FALSE(xw.last_error());
}
