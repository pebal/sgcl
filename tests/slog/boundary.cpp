//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// slog at its boundaries (DESIGN 408): every handle moved from and assigned
// to itself, a logger of an empty writer and of an empty handler, a
// handler, a writer and a value of the program that throw, an empty or a
// null message and key, values of a megabyte and lines at the size of a
// batch, the extreme levels, sampling at its limits, groups nested past
// the depth the module follows a value of the program to, the default
// records: record, attrs, value, message. What the formats write is
// compared with Go in oracle.cpp; the rest of the behaviour is in
// logger.cpp.
#include "common.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using slog_test::text_of;

namespace {
    constexpr int64_t At = 1790604301123456789;   // 2026-09-28T14:05:01.123456789Z

    // A writer that counts its writes and keeps what they wrote
    struct Counting {
        std::atomic<int> writes{0};
        std::string bytes;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            bytes.append(reinterpret_cast<const char*>(d.data()), d.size());
            ++writes;
            return d.size();
        }
    };

    // A writer of the program whose first `fail` writes throw
    struct Throwing {
        int fail = 1;
        std::string bytes;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            if (fail > 0) {
                --fail;
                throw std::runtime_error("the writer threw");
            }
            bytes.append(reinterpret_cast<const char*>(d.data()), d.size());
            return d.size();
        }
    };

    // What file descriptor 2 gets while f runs, through a pipe
    template<class F>
    std::string stderr_of(F&& f) {
        int pipe_fd[2];
        EXPECT_EQ(::pipe(pipe_fd), 0);
        const int saved = ::dup(2);
        ::dup2(pipe_fd[1], 2);
        f();
        ::dup2(saved, 2);
        ::close(saved);
        ::close(pipe_fd[1]);
        std::string out;
        char buf[4096];
        ssize_t n;
        while ((n = ::read(pipe_fd[0], buf, sizeof buf)) > 0) {
            out.append(buf, size_t(n));
        }
        ::close(pipe_fd[0]);
        return out;
    }

    size_t lines_in(const std::string& s) {
        size_t n = 0;
        for (char c : s) {
            n += c == '\n';
        }
        return n;
    }

    std::string str(const slice<const char>& s) {
        return std::string(s.data(), s.size());
    }

    std::string str(const string& s) {
        return std::string(s.data(), s.size());
    }

    // The part of a line after " msg=", without the new line
    std::string after_msg(const std::string& line) {
        auto at = line.find(" msg=");
        auto end = line.find('\n', at);
        return at == std::string::npos ? line : line.substr(at + 5, end - at - 5);
    }

    // The part of a JSON line after "msg":, without the new line
    std::string after_json_msg(const std::string& line) {
        auto at = line.find(",\"msg\":");
        auto end = line.find('\n', at);
        return at == std::string::npos ? line : line.substr(at + 7, end - at - 7);
    }

    // Groups nested N + 1 deep, the innermost holding v=1: each level a
    // member of the next, so that each group refers to a live one
    template<int N>
    struct Nest {
        Nest<N - 1> inner;
        slog::group<decltype(inner.g)> g{"g", inner.g};
    };

    template<>
    struct Nest<0> {
        int v = 1;
        slog::group<char[2], int> g{"g", "v", v};
    };

    std::string repeated(const std::string& s, int n) {
        std::string out;
        for (int i = 0; i < n; ++i) {
            out += s;
        }
        return out;
    }

    // The key path of nested groups named g, down a kept record: the depth
    // reached, and the int found at the bottom (-1 for none)
    void walk(const slog::value& v, int& depth, int64_t& bottom) {
        for (auto a : v.as_group()) {
            if (a.value().type() == slog::value::kind::group) {
                ++depth;
                walk(a.value(), depth, bottom);
            } else if (a.value().type() == slog::value::kind::int64) {
                bottom = a.value().as_int();
            }
        }
    }

    // A described type whose pointer leads back to itself
    struct Node {
        int v = 1;
        tracked_ptr<Node> next;

        void describe(encoding::field_list& f) {
            f.add("v", v);
            f.add("next", next);
        }
    };
}

//--------------------------------------------------------------------
// logger
//--------------------------------------------------------------------
TEST(SlogBoundary_Tests, ALoggerMovedFromIsTheSameLogger) {
    // a move copies the word, as a move of a tracked_ptr does: the logger
    // moved from shares the output and the attributes still
    io::buffer out;
    auto a = slog::logger(out).with("k", 1);
    auto b = std::move(a);
    a.info("from a");
    b.info("from b");
    auto text = text_of(out);
    EXPECT_NE(text.find("msg=\"from a\" k=1\n"), std::string::npos) << text;
    EXPECT_NE(text.find("msg=\"from b\" k=1\n"), std::string::npos) << text;
    EXPECT_TRUE(a.enabled(slog::level::info));
    EXPECT_EQ(a.dropped(), 0u);
    a.flush();
    a.group("g").with("x", 2).info("child");
    EXPECT_NE(text_of(out).find("msg=child k=1 g.x=2\n"), std::string::npos);
    // assigned to itself, by copy and by move
    auto& same = b;
    b = same;
    b = std::move(same);
    out.clear();
    b.info("self");
    EXPECT_EQ(after_msg(text_of(out)), "self k=1");
}

TEST(SlogBoundary_Tests, ALoggerOfAnEmptyWriterCountsItsRecordsAsLost) {
    // An empty io::writer is no output: the record is lost, counted by
    // dropped() and said once on stderr, as a failed write is
    std::string err = stderr_of([] {
        slog::logger lg{io::writer()};
        lg.info("to nothing");
        lg.warn("again");
        EXPECT_EQ(lg.dropped(), 2u);
        auto json = slog::logger(slog::options{.out = io::writer(), .json = true});
        json.error("to nothing");
        EXPECT_EQ(json.dropped(), 1u);
        auto batched = slog::logger(slog::options{.out = io::writer(), .buffered = true});
        batched.info("a");
        batched.info("b");
        batched.flush();
        EXPECT_EQ(batched.dropped(), 2u);
    });
    EXPECT_EQ(lines_in(err), 3u) << err;   // once per output
    EXPECT_NE(err.find("sgcl::slog: a write failed: "), std::string::npos) << err;
}

TEST(SlogBoundary_Tests, ALoggerOfAnEmptyHandlerWritesTextOnStderr) {
    // constructor (4) with no handler: as options with no handler, the
    // lines go to options::out, io::stderr
    std::string err = stderr_of([] {
        slog::logger lg{slog::handler()};
        EXPECT_TRUE(lg.enabled(slog::level::info));
        EXPECT_FALSE(lg.enabled(slog::level::debug));
        lg.info("on stderr", "k", 1);
        EXPECT_EQ(lg.dropped(), 0u);
    });
    EXPECT_EQ(after_msg(err), "\"on stderr\" k=1") << err;
}

TEST(SlogBoundary_Tests, AHandlerThatThrows) {
    struct Throws {
        int* calls;
        bool* refuse;

        void handle(const slog::record& r) const {
            ++*calls;
            if (str(r.message()) == "throw") {
                throw std::runtime_error("the handler threw");
            }
        }

        bool enabled(slog::level) const {
            if (*refuse) {
                throw std::runtime_error("enabled threw");
            }
            return true;
        }
    };
    int calls = 0;
    bool refuse = false;
    auto lg = slog::logger(Throws{&calls, &refuse});
    EXPECT_THROW(lg.info("throw", "k", 1), std::runtime_error);
    lg.info("next");   // the logger goes on
    EXPECT_EQ(calls, 2);
    refuse = true;
    EXPECT_THROW((void)lg.enabled(slog::level::info), std::runtime_error);
    EXPECT_THROW(lg.info("asked first"), std::runtime_error);
    EXPECT_EQ(calls, 2);   // the record was not made
    refuse = false;
    lg.with("a", 1).group("g").info("after");
    EXPECT_EQ(calls, 3);
}

TEST(SlogBoundary_Tests, AValueThatThrowsLeavesTheLoggerAsItWas) {
    struct BadText {
        string to_text() const {
            throw std::runtime_error("to_text threw");
        }
    };
    struct BadFormat {
        int n = 0;
    };
    io::buffer out;
    auto lg = slog::logger(slog::options{.out = out, .utc = true});
    EXPECT_THROW(lg.info("m", "k", BadText()), std::runtime_error);
    EXPECT_THROW((void)lg.with("k", BadText()), std::runtime_error);
    lg.info("after", "k", 1);   // the thread's line is free again, nothing of the failed record in it
    EXPECT_EQ(lines_in(text_of(out)), 1u);
    EXPECT_EQ(after_msg(text_of(out)), "after k=1");
}

TEST(SlogBoundary_Tests, AWriterThatThrows) {
    // unbuffered: the exception comes out of the verb, the next record is
    // written
    Throwing w;
    auto lg = slog::logger(w);
    EXPECT_THROW(lg.info("lost"), std::runtime_error);
    lg.info("written");
    EXPECT_EQ(lines_in(w.bytes), 1u);
    EXPECT_EQ(after_msg(w.bytes), "written");
    // buffered: out of the warn that writes the batch, and of flush(); the
    // batch is usable after either (its lock let go), the throwing write's
    // lines lost
    Throwing b;
    auto batched = slog::logger(slog::options{.out = b, .buffered = true});
    batched.info("one");
    EXPECT_THROW(batched.warn("two"), std::runtime_error);
    std::atomic<bool> done{false};
    std::thread t([&] {
        batched.info("three");
        b.fail = 1;
        try {
            batched.flush();
        } catch (const std::runtime_error&) {
            done = true;
        }
        batched.info("four");
        batched.flush();
    });
    for (int i = 0; i < 5000 && !done; ++i) {   // at most 5 s: a slot left locked hangs the thread
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(done.load()) << "the batch stayed locked after the writer threw";
    t.join();
    EXPECT_EQ(after_msg(b.bytes), "four");
    EXPECT_EQ(lines_in(b.bytes), 1u);
    // a worker's batch written on its way to sleep, where nobody takes the
    // exception: its lines counted as lost
    Throwing idle;
    auto by_worker = slog::logger(slog::options{.out = idle, .buffered = true});
    async::spawn([by_worker]() -> async::task<> {
        by_worker.info("one");
        by_worker.info("two");
        co_return;
    }).wait();
    for (int i = 0; i < 5000 && by_worker.dropped() == 0; ++i) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(by_worker.dropped(), 2u);
    by_worker.info("three");
    by_worker.flush();
    EXPECT_EQ(after_msg(idle.bytes), "three");
}

TEST(SlogBoundary_Tests, AnEmptyOrNullMessageAndKey) {
    slog_test::FixedTime at(At);
    const char* none = nullptr;
    auto [text, json] = slog_test::both([&](const slog::logger& l) {
        l.info(none, none, 1, "", 2);
        l.info("", "k", "");
        l.with(none, 3).group(none).group("").info(std::string());
    });
    EXPECT_EQ(text,
              "time=2026-09-28T14:05:01.123Z level=INFO msg=\"\" \"\"=1 \"\"=2\n"
              "time=2026-09-28T14:05:01.123Z level=INFO msg=\"\" k=\"\"\n"
              "time=2026-09-28T14:05:01.123Z level=INFO msg=\"\" \"\"=3\n");
    EXPECT_EQ(json,
              "{\"time\":\"2026-09-28T14:05:01.123456789Z\",\"level\":\"INFO\",\"msg\":\"\",\"\":1,\"\":2}\n"
              "{\"time\":\"2026-09-28T14:05:01.123456789Z\",\"level\":\"INFO\",\"msg\":\"\",\"k\":\"\"}\n"
              "{\"time\":\"2026-09-28T14:05:01.123456789Z\",\"level\":\"INFO\",\"msg\":\"\",\"\":3}\n");
    // a handler sees them empty
    slog::memory kept;
    slog::logger(kept).info(none, none, 1);
    auto r = kept.records()[0];
    EXPECT_TRUE(r.message().empty());
    ASSERT_EQ(r.size(), 1u);
    EXPECT_TRUE((*r.begin()).key().empty());
    EXPECT_EQ((*r.begin()).value().as_int(), 1);
}

TEST(SlogBoundary_Tests, ValuesOfAMegabyte) {
    slog_test::FixedTime at(At);
    const std::string plain(1 << 20, 'x');
    const std::string lines(1 << 20, '\n');   // each byte escaped as two
    Counting w;
    auto lg = slog::logger(slog::options{.out = w, .utc = true});
    lg.info("m", "v", plain);
    lg.info("m", "v", lines);
    lg.info(plain);
    lg.info("m", plain.c_str(), 1);
    lg.info("small");   // the line grown to the largest record still makes a small one
    EXPECT_EQ(w.writes.load(), 5);
    const std::string head = "time=2026-09-28T14:05:01.123Z level=INFO msg=";
    const std::string expect = head + "m v=" + plain + "\n" + head + "m v=\"" + repeated("\\n", 1 << 20) + "\"\n" + head + plain + "\n" + head
                             + "m " + plain + "=1\n" + head + "small\n";
    EXPECT_TRUE(w.bytes == expect);
    // JSON: a control byte as \u00XX, six bytes each
    Counting j;
    const std::string controls(1 << 20, '\x01');
    slog::logger(slog::options{.out = j, .json = true, .utc = true}).info("m", "v", controls);
    EXPECT_EQ(j.bytes.size(), after_json_msg(j.bytes).size() + std::string("{\"time\":\"2026-09-28T14:05:01.123456789Z\",\"level\":\"INFO\",\"msg\":").size() + 1);
    EXPECT_EQ(after_json_msg(j.bytes), "\"m\",\"v\":\"" + repeated("\\u0001", 1 << 20) + "\"}");
    // kept by with() and by a clone, the copies whole
    io::buffer out;
    slog::logger(slog::options{.out = out, .utc = true}).with("v", plain).info("m");
    EXPECT_TRUE(after_msg(text_of(out)) == "m v=" + plain);
    slog::memory kept;
    slog::logger(kept).info("m", "v", plain);
    EXPECT_TRUE(str((*kept.records()[0].begin()).value().as_string()) == plain);
}

TEST(SlogBoundary_Tests, LinesAtTheSizeOfABatch) {
    slog_test::FixedTime at(At);
    constexpr size_t Batch = 32 * 1024;
    Counting w;
    auto lg = slog::logger(slog::options{.out = w, .utc = true, .buffered = true});
    // the length of a line around a value of n bytes
    lg.info("m", "v", "a");
    lg.flush();
    const size_t around = w.bytes.size() - 1;
    w.bytes.clear();
    w.writes = 0;
    // a line of exactly a batch waits in it
    lg.info("m", "v", std::string(Batch - around, 'x'));
    EXPECT_EQ(w.writes.load(), 0);
    lg.flush();
    EXPECT_EQ(w.writes.load(), 1);
    EXPECT_EQ(w.bytes.size(), Batch);
    // a byte more is written alone, at once
    lg.info("m", "v", std::string(Batch - around + 1, 'x'));
    EXPECT_EQ(w.writes.load(), 2);
    EXPECT_EQ(w.bytes.size(), 2 * Batch + 1);
    // a line that fills the rest of a batch exactly, then one byte more
    lg.info("m", "v", std::string(100, 'x'));
    lg.info("m", "v", std::string(Batch - 2 * around - 100, 'x'));
    EXPECT_EQ(w.writes.load(), 2);
    lg.info("m", "v", "a");   // does not fit: the full batch first
    EXPECT_EQ(w.writes.load(), 3);
    lg.flush();
    EXPECT_EQ(w.writes.load(), 4);
    EXPECT_EQ(w.bytes.size(), 3 * Batch + 1 + around + 1);
    EXPECT_EQ(lines_in(w.bytes), 5u);
}

TEST(SlogBoundary_Tests, TheExtremeLevels) {
    slog_test::FixedTime at(At);
    io::buffer out;
    auto all = slog::logger(slog::options{.out = out, .level = slog::level(INT8_MIN), .utc = true});
    all.log(slog::level(INT8_MIN), "m");
    all.log(slog::level(INT8_MAX), "m");
    auto text = text_of(out);
    EXPECT_NE(text.find("level=DEBUG-124 msg=m\n"), std::string::npos) << text;   // Go's Level.String
    EXPECT_NE(text.find("level=ERROR+119 msg=m\n"), std::string::npos) << text;
    out.clear();
    slog::logger(slog::options{.out = out, .level = slog::level(INT8_MIN), .json = true, .utc = true}).log(slog::level(INT8_MAX), "m");
    EXPECT_NE(text_of(out).find("\"level\":\"ERROR+119\""), std::string::npos);
    auto top = slog::logger(out, slog::level(INT8_MAX));
    EXPECT_FALSE(top.enabled(slog::level::error));
    EXPECT_TRUE(top.enabled(slog::level(INT8_MAX)));
    slog::level_var v(slog::level(INT8_MIN));
    auto by_var = slog::logger(slog::options{.out = out, .level_var = v});
    EXPECT_TRUE(by_var.enabled(slog::level(INT8_MIN)));
    v.set(slog::level(INT8_MAX));
    EXPECT_EQ(v.get(), slog::level(INT8_MAX));
    EXPECT_FALSE(by_var.enabled(slog::level(INT8_MAX - 1)));
    EXPECT_TRUE(by_var.enabled(slog::level(INT8_MAX)));
    // a level_var moved from is the same variable
    auto moved = std::move(v);
    v.set(slog::level::warn);
    EXPECT_EQ(moved.get(), slog::level::warn);
    auto& same = v;
    v = same;
    EXPECT_EQ(v.get(), slog::level::warn);
}

TEST(SlogBoundary_Tests, SamplingAtItsLimits) {
    slog_test::FixedTime at(At);
    io::buffer out;
    // a span of zero or below is no sampling
    auto none = slog::logger(slog::options{.out = out, .sample_first = 0, .sample_per = -1s});
    for (int i = 0; i < 3; ++i) {
        none.info("same");
    }
    EXPECT_EQ(lines_in(text_of(out)), 3u);
    out.clear();
    // none first, none then: every record left out, and said in the next span
    auto quiet = slog::logger(slog::options{.out = out, .utc = true, .sample_per = 1s});
    for (int i = 0; i < 4; ++i) {
        quiet.info("same");
    }
    EXPECT_EQ(lines_in(text_of(out)), 0u);
    at.clock.advance(1s);
    quiet.info("other");
    EXPECT_EQ(text_of(out), "time=2026-09-28T14:05:02.123Z level=WARN msg=\"records sampled out\" count=4\n");
    out.clear();
    // every one: first 0, then 1
    auto every = slog::logger(slog::options{.out = out, .sample_then = 1, .sample_per = 1s});
    for (int i = 0; i < 5; ++i) {
        every.info("same");
    }
    EXPECT_EQ(lines_in(text_of(out)), 5u);
    out.clear();
    // the largest first
    auto most = slog::logger(slog::options{.out = out, .sample_first = UINT32_MAX, .sample_per = sgcl::duration::max()});
    for (int i = 0; i < 5; ++i) {
        most.info("same");
    }
    EXPECT_EQ(lines_in(text_of(out)), 5u);
}

//--------------------------------------------------------------------
// groups at the depth limit
//--------------------------------------------------------------------
TEST(SlogBoundary_Tests, GroupsNestedPastSixtyFourLevels) {
    // groups of the program are its own, finite: every level is written,
    // in both formats, and kept whole by with() and by a clone; the depth
    // the module stops at (64) is for a value of the program that points
    // back to itself
    slog_test::FixedTime at(At);
    Nest<69> deep;   // 70 groups
    auto [text, json] = slog_test::both([&](const slog::logger& l) {
        l.info("m", deep.g);
        l.with(deep.g).info("m");
    });
    const std::string line_text = "m " + repeated("g.", 70) + "v=1";
    const std::string line_json = "\"m\"," + repeated("\"g\":{", 70) + "\"v\":1" + repeated("}", 70) + "}";
    EXPECT_EQ(lines_in(text), 2u);
    EXPECT_EQ(after_msg(text.substr(0, text.find('\n') + 1)), line_text);
    EXPECT_EQ(after_msg(text.substr(text.find('\n') + 1)), line_text);
    EXPECT_EQ(after_json_msg(json.substr(0, json.find('\n') + 1)), line_json);
    EXPECT_EQ(after_json_msg(json.substr(json.find('\n') + 1)), line_json);
    slog::memory kept;
    slog::logger(kept).info("m", deep.g);
    int depth = 1;
    int64_t bottom = -1;
    walk((*kept.records()[0].begin()).value(), depth, bottom);
    EXPECT_EQ(depth, 70);
    EXPECT_EQ(bottom, 1);
}

TEST(SlogBoundary_Tests, ALoggerInSeventyGroups) {
    io::buffer out;
    slog::memory kept;
    auto lg = slog::logger(slog::options{.out = out, .json = true});
    auto to_kept = slog::logger(kept);
    for (int i = 0; i < 70; ++i) {
        lg = lg.group("g");
        to_kept = to_kept.group("g");
    }
    lg.info("m", "v", 1);
    EXPECT_EQ(after_json_msg(text_of(out)), "\"m\"," + repeated("\"g\":{", 70) + "\"v\":1" + repeated("}", 70) + "}");
    to_kept.info("m", "v", 1);
    int depth = 1;
    int64_t bottom = -1;
    walk((*kept.records()[0].begin()).value(), depth, bottom);
    EXPECT_EQ(depth, 70);
    EXPECT_EQ(bottom, 1);
}

TEST(SlogBoundary_Tests, AValueThatPointsBackToItself) {
    // followed 64 levels deep in both formats, by with() and by a clone
    // alike; the level past the limit is left out whole
    slog_test::FixedTime at(At);
    tracked_ptr<Node> node = make_tracked<Node>();
    node->next = node;
    auto [text, json] = slog_test::both([&](const slog::logger& l) {
        l.info("m", "n", *node);
        l.with("n", *node).info("m");
    });
    const std::string first_text = text.substr(0, text.find('\n') + 1);
    EXPECT_EQ(first_text, text.substr(text.find('\n') + 1));
    const std::string first_json = json.substr(0, json.find('\n') + 1);
    EXPECT_EQ(first_json, json.substr(json.find('\n') + 1));
    // as many values as objects followed, the same in both formats, none
    // an empty group
    size_t in_text = 0, in_json = 0;
    for (size_t at = 0; (at = first_text.find("v=1", at)) != std::string::npos; ++at) {
        ++in_text;
    }
    for (size_t at = 0; (at = first_json.find("\"v\":1", at)) != std::string::npos; ++at) {
        ++in_json;
    }
    EXPECT_EQ(in_text, 65u);
    EXPECT_EQ(in_json, in_text);
    EXPECT_EQ(first_json.find("{}"), std::string::npos) << first_json;
    EXPECT_EQ(first_text.find("<nil>"), std::string::npos);
    auto parsed = encoding::json::parse(string(first_json));
    EXPECT_TRUE(parsed.has_value());
    // a clone: the same levels, nothing past them
    slog::memory kept;
    slog::logger(kept).info("m", "n", *node);
    int depth = 1;
    int64_t bottom = -1;
    walk((*kept.records()[0].begin()).value(), depth, bottom);
    EXPECT_EQ(depth, 65);
    node->next = nullptr;   // the cycle let go of
}

//--------------------------------------------------------------------
// a kept text past the 32 bits of its length
//--------------------------------------------------------------------
// The text form of a value of the program that with() and clone() keep has
// its length in 32 bits: a longer one is cut to fit, at the start of a
// code point, rather than its length wrapped (the cut itself, at any
// limit)
TEST(SlogBoundary_Tests, AKeptTextIsCutAtTheStartOfACodePoint) {
    using slog::detail::cut_text;
    EXPECT_EQ(cut_text("abc", 3, 3), 3u);
    EXPECT_EQ(cut_text("abc", 3, 10), 3u);
    EXPECT_EQ(cut_text("abcd", 4, 3), 3u);
    EXPECT_EQ(cut_text("abc", 3, 0), 0u);
    EXPECT_EQ(cut_text("", 0, 0), 0u);
    EXPECT_EQ(cut_text("a\xc3\xa9", 3, 2), 1u);              // é cut in two: back before it
    EXPECT_EQ(cut_text("a\xc3\xa9" "b", 4, 3), 3u);             // é whole
    EXPECT_EQ(cut_text("a\xf0\x9f\x98\x80" "b", 6, 3), 1u);   // a four-byte code point cut at any byte
    EXPECT_EQ(cut_text("a\xf0\x9f\x98\x80" "b", 6, 4), 1u);
    EXPECT_EQ(cut_text("a\xf0\x9f\x98\x80" "b", 6, 5), 5u);
    EXPECT_EQ(cut_text("\x80\x80\x80\x80\x80\x80", 6, 5), 2u);   // ill-formed: three bytes back at most
}

namespace {
    // A container whose %+v text passes 4 GiB from little memory: the
    // pointers lead to one vector of NaNs (4 bytes of text each), and the
    // first NaN fails its JSON, which is then slog's short error string
    struct Wide {
        vector<tracked_ptr<vector<double>>> v;

        void describe(encoding::field_list& f) {
            f.add("v", v);
        }
    };
}

// The same through clone(), a text of 4.3 GiB: about 9 GB of memory for a
// few seconds, so not under a sanitizer nor on a machine under 16 GB
TEST(SlogBoundary_Tests, AKeptTextPastFourGiBIsCut) {
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    GTEST_SKIP() << "4 GiB of text under a sanitizer";
#endif
#endif
    if (sgcl::detail::os::physical_memory() < (size_t(16) << 30)) {
        GTEST_SKIP() << "under 16 GB of memory";
    }
    tracked_ptr<vector<double>> nans = make_tracked<vector<double>>(size_t(1) << 20, std::nan(""));
    Wide wide;
    for (int i = 0; i < 1100; ++i) {   // 1100 times 4 MiB of text
        wide.v.push_back(nans);
    }
    slog::memory kept;
    slog::logger(kept).info("m", "r", wide);
    wide.v.clear();
    auto r = kept.records()[0];
    auto group = (*r.begin()).value().as_group();
    ASSERT_EQ(group.size(), 1u);
    const auto v = *group.begin();
    EXPECT_EQ(v.value().type(), slog::value::kind::any);
    EXPECT_EQ(str(v.value().json()), "\"!ERROR:json: unsupported value: NaN\"");
    const slog::detail::Value& kept_value = slog::detail::Access::of(v)->value;
    EXPECT_EQ(kept_value.count, UINT32_MAX);   // the length wrapped to about 319 MB before
    const std::string_view text(kept_value.s.p + kept_value.s.n, kept_value.count);
    EXPECT_EQ(text.substr(0, 10), "[[NaN NaN ");
    EXPECT_EQ(text.substr(text.size() - 4).find_first_not_of("NaN "), std::string_view::npos);
}

//--------------------------------------------------------------------
// handler and memory
//--------------------------------------------------------------------

TEST(SlogBoundary_Tests, AnEmptyHandler) {
    slog::handler h;
    EXPECT_FALSE(bool(h));
    EXPECT_THROW(h.handle(slog::record()), std::logic_error);
    EXPECT_THROW((void)h.enabled(slog::level::info), std::logic_error);
    auto copy = h;
    EXPECT_FALSE(bool(copy));
}

TEST(SlogBoundary_Tests, AHandlerAndAMemoryMovedFromAreTheSame) {
    slog::memory kept;
    slog::handler h(kept);
    auto moved = std::move(h);
    EXPECT_TRUE(bool(h));
    h.handle(slog::record());
    moved.handle(slog::record());
    EXPECT_EQ(kept.size(), 2u);
    auto& same = h;
    h = same;
    h = std::move(same);
    EXPECT_TRUE(h.enabled(slog::level(INT8_MIN)));
    auto other = std::move(kept);
    kept.clear();
    EXPECT_EQ(other.size(), 0u);
    auto& itself = kept;
    kept = itself;
    kept.clear();   // twice, of nothing
    EXPECT_TRUE(kept.records().empty());
}

//--------------------------------------------------------------------
// record, attrs, value, message
//--------------------------------------------------------------------
TEST(SlogBoundary_Tests, TheDefaultRecordAndItsViews) {
    slog::record r;
    EXPECT_EQ(r.level(), slog::level::info);
    EXPECT_TRUE(r.message().empty());
    EXPECT_FALSE(r.has_source());
    EXPECT_EQ(r.source().line(), 0u);
    EXPECT_EQ(r.time().unix_nano(), 0);
    EXPECT_TRUE(r.empty());
    EXPECT_EQ(r.size(), 0u);
    EXPECT_TRUE(r.begin() == r.end());
    auto c = r.clone();
    EXPECT_TRUE(c.empty());
    EXPECT_TRUE(c.message().empty());
    auto cc = c.clone();   // a clone of a clone
    EXPECT_EQ(cc.level(), slog::level::info);
    slog::attrs::iterator i;
    EXPECT_TRUE(i == slog::attrs().end());
    slog::value v;
    EXPECT_EQ(v.type(), slog::value::kind::null);
    EXPECT_EQ(str(v.text()), "<nil>");
    EXPECT_EQ(str(v.json()), "null");
    EXPECT_THROW((void)v.as_bool(), std::logic_error);
    EXPECT_THROW((void)v.as_int(), std::logic_error);
    EXPECT_THROW((void)v.as_uint(), std::logic_error);
    EXPECT_THROW((void)v.as_double(), std::logic_error);
    EXPECT_THROW((void)v.as_string(), std::logic_error);
    EXPECT_THROW((void)v.as_duration(), std::logic_error);
    EXPECT_THROW((void)v.as_time(), std::logic_error);
    EXPECT_THROW((void)v.as_group(), std::logic_error);
}

TEST(SlogBoundary_Tests, TheValuesOfARecordAtTheirLimits) {
    slog::memory kept;
    slog::logger(kept).info("m", "i", INT64_MIN, "u", UINT64_MAX, "d", sgcl::duration::min(), slog::group("e"), "b", true);
    auto r = kept.records()[0];
    auto it = r.begin();
    EXPECT_EQ((*it).value().as_int(), INT64_MIN);
    EXPECT_EQ((*++it).value().as_uint(), UINT64_MAX);
    EXPECT_EQ((*++it).value().as_duration(), sgcl::duration::min());
    auto e = (*++it).value();
    EXPECT_EQ(e.type(), slog::value::kind::group);   // an empty group is kept, and left out by the formats
    EXPECT_TRUE(e.as_group().empty());
    EXPECT_EQ(str(e.text()), "[]");
    EXPECT_EQ(str(e.json()), "{}");
    EXPECT_TRUE((*++it).value().as_bool());
    EXPECT_TRUE(++it == r.end());
}

TEST(SlogBoundary_Tests, MessagesAtTheirBoundaries) {
    const char* none = nullptr;
    EXPECT_TRUE(slog::message(none).text().empty());
    EXPECT_TRUE(slog::message("").text().empty());
    const char unterminated[3] = {'a', 'b', 'c'};   // never read past its end
    EXPECT_EQ(str(slog::message(unterminated).text()), "abc");
    const char cut[4] = {'a', '\0', 'b', '\0'};   // to the first NUL
    EXPECT_EQ(str(slog::message(cut).text()), "a");
    const std::string inner("a\0b", 3);   // a std::string keeps its NUL
    EXPECT_EQ(slog::message(inner).text().size(), 3u);
    io::buffer out;
    slog::logger(out).info(inner);
    EXPECT_EQ(after_msg(text_of(out)), "\"a\\x00b\"");
}
