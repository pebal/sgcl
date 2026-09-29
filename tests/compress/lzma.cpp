//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// LZMA "alone" (.lzma) against liblzma, xz's library, both ways: what we
// make decoded by lzma_alone_decoder, what its presets 0..9 and -e make
// decoded by us, on the corpus of common.h and files of this repository;
// the size known with no marker, not known with the marker, and known with
// the marker too; damaged data, the limits, the streams sync and async.
#include "common.h"

#include <filesystem>

#ifndef SGCL_TEST_LIBLZMA
#define SGCL_TEST_LIBLZMA 0
#endif
#if SGCL_TEST_LIBLZMA
#include <lzma.h>
#endif

using namespace compress_test;
using compress::lzma;

namespace {
    std::string repo_file(const std::string& name) {
        auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
        std::ifstream is(root / name, std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }

    // Headers of the library one after another: text of a few MB
    std::string repo_text(size_t limit) {
        auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "sgcl";
        std::vector<std::filesystem::path> files;
        for (auto& e : std::filesystem::recursive_directory_iterator(root)) {
            if (e.path().extension() == ".h") {
                files.push_back(e.path());
            }
        }
        std::sort(files.begin(), files.end());
        std::string all;
        for (auto& f : files) {
            std::ifstream is(f, std::ios::binary);
            std::stringstream ss;
            ss << is.rdbuf();
            all += ss.str();
            if (all.size() >= limit) {
                break;
            }
        }
        all.resize(std::min(all.size(), limit));
        return all;
    }

    std::vector<std::pair<std::string, std::string>> lzma_corpus() {
        auto c = corpus();
        c.push_back({"zeros", std::string(300000, '\0')});
        std::string rep;
        while (rep.size() < 200000) {
            rep += "abcdefghij0123456789-repeat-";
        }
        c.push_back({"repeats", rep});
        std::mt19937 rng(11);
        for (size_t n : {size_t(65535), size_t(65536), size_t(65537)}) {
            std::string t(n, 0);
            for (auto& ch : t) {
                ch = char(rng() % 4 == 0 ? rng() : 'a' + rng() % 3);
            }
            c.push_back({"mixed" + std::to_string(n), t});
        }
        c.push_back({"README.md", repo_file("README.md")});
        return c;
    }

    // A writer that takes `room` bytes and then fails: EIO the first time,
    // ENOSPC every time after, so that a failure handed on is told from a
    // new one; calls counts the writes tried
    class failing_writer final : public sgcl::io::mixin::writer<failing_writer> {
    public:
        using sgcl::io::mixin::writer<failing_writer>::write;
        using sgcl::io::mixin::writer<failing_writer>::async_write;

        explicit failing_writer(size_t room) : _room(room) {}

        sgcl::expected<size_t, sgcl::io::error> write(const sgcl::slice<const std::byte>& data) {
            ++calls;
            if (failures || taken + data.size() > _room) {
                ++failures;
                return sgcl::unexpected<sgcl::io::error>(sgcl::io::error(sgcl::error_code(failures == 1 ? EIO : ENOSPC, std::system_category()), "write", "disk"));
            }
            taken += data.size();
            return data.size();
        }

        sgcl::async::task<sgcl::expected<size_t, sgcl::io::error>> async_write(sgcl::slice<const std::byte> data) {
            co_return write(data);
        }

        size_t taken = 0;
        int calls = 0;
        int failures = 0;

    private:
        size_t _room;
    };

    std::string buffer_text(const sgcl::io::buffer& b) {
        return std::string(reinterpret_cast<const char*>(b.data().data()), b.size());
    }

    // Everything a reader gives, read `step` bytes at a time
    template<class R>
    bool read_everything(R& r, size_t step, std::string& out) {
        std::vector<std::byte> buf(step);
        out.clear();
        for (;;) {
            auto n = r.read(sgcl::slice<std::byte>(buf.data(), buf.size()));
            if (!n) {
                return false;
            }
            if (*n == 0) {
                return true;
            }
            out.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
    }

#if SGCL_TEST_LIBLZMA
    std::string xz_run(lzma_stream& s, const std::string& in, bool& ok) {
        std::string out;
        std::string buf(1 << 16, 0);
        s.next_in = reinterpret_cast<const uint8_t*>(in.data());
        s.avail_in = in.size();
        lzma_ret r;
        do {
            s.next_out = reinterpret_cast<uint8_t*>(buf.data());
            s.avail_out = buf.size();
            r = lzma_code(&s, LZMA_FINISH);
            out.append(buf.data(), buf.size() - s.avail_out);
        } while (r == LZMA_OK);
        ok = r == LZMA_STREAM_END;
        lzma_end(&s);
        return out;
    }

    // liblzma's .lzma of the preset (LZMA_PRESET_EXTREME or'd in for -e):
    // the size not known, the end marker
    std::string xz_encode(const std::string& in, uint32_t preset) {
        lzma_options_lzma o;
        lzma_lzma_preset(&o, preset);
        lzma_stream s = LZMA_STREAM_INIT;
        EXPECT_EQ(lzma_alone_encoder(&s, &o), LZMA_OK);
        bool ok;
        auto out = xz_run(s, in, ok);
        EXPECT_TRUE(ok);
        return out;
    }

    bool xz_decode(const std::string& in, std::string& out) {
        lzma_stream s = LZMA_STREAM_INIT;
        if (lzma_alone_decoder(&s, UINT64_MAX) != LZMA_OK) {
            return false;
        }
        bool ok;
        out = xz_run(s, in, ok);
        return ok;
    }
#endif

#define SKIP_WITHOUT_LIBLZMA() \
    if (!SGCL_TEST_LIBLZMA) GTEST_SKIP() << "liblzma not found (SGCL_XZ_ROOT)"
}

// What we make at every level, liblzma decoding it and we decoding it
TEST(Lzma_Tests, OursDecodeWithLiblzmaAtEveryLevel) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    for (auto& [name, t] : lzma_corpus()) {
        for (int level = 0; level <= 9; ++level) {
            for (bool extreme : {false, true}) {
                if (extreme && level != 0 && level != 3 && level != 6 && level != 9) {
                    continue;
                }
                auto c = lzma::compress(bytes(t), {.level = level, .extreme = extreme});
                std::string back;
                ASSERT_TRUE(xz_decode(text(c), back)) << name << " level " << level << (extreme ? "e" : "");
                ASSERT_EQ(back, t) << name << " level " << level;
                auto ours = lzma::decompress(c);
                ASSERT_TRUE(ours) << name << " level " << level << ": " << ours.error().message();
                ASSERT_EQ(text(*ours), t) << name << " level " << level;
            }
        }
    }
#endif
}

// A text of 3 MB, larger than the fast levels' dictionaries, and within 2%
// of liblzma's size at the same preset
TEST(Lzma_Tests, ALargeTextAndItsSize) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    auto t = repo_text(size_t(3) << 20);
    ASSERT_GT(t.size(), size_t(2) << 20);
    for (int level : {0, 3, 6}) {
        auto c = lzma::compress(bytes(t), {.level = level});
        std::string back;
        ASSERT_TRUE(xz_decode(text(c), back)) << level;
        ASSERT_EQ(back, t);
        auto x = xz_encode(t, uint32_t(level));
        EXPECT_LT(double(c.size()), double(x.size()) * 1.02) << "level " << level << ": " << c.size() << " against " << x.size();
    }
#endif
}

// What liblzma's presets make, decoded by us in memory and as a stream
TEST(Lzma_Tests, LiblzmaPresetsDecodeHere) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    for (auto& [name, t] : lzma_corpus()) {
        for (uint32_t preset : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 0u | LZMA_PRESET_EXTREME, 6u | LZMA_PRESET_EXTREME, 9u | LZMA_PRESET_EXTREME}) {
            auto x = xz_encode(t, preset);
            auto back = lzma::decompress(bytes(x));
            ASSERT_TRUE(back) << name << " preset " << preset << ": " << back.error().message();
            ASSERT_EQ(text(*back), t) << name << " preset " << preset;
            if (preset == 6) {
                lzma::reader r(dribble{x, 777});
                std::string got;
                ASSERT_TRUE(read_everything(r, 1000, got)) << name;
                ASSERT_EQ(got, t) << name;
            }
        }
    }
#endif
}

// The three forms of the size: known with no marker (compress), not known
// with the marker (a writer), and known with the marker as well (liblzma's
// stream with the size written into its header), in memory and as a stream
TEST(Lzma_Tests, TheSizeKnownOrNotAndTheMarker) {
    auto t = read_oracle("compress/e.txt");
    auto known = text(lzma::compress(bytes(t)));
    EXPECT_EQ(uint8_t(known[5]), uint8_t(t.size()));
    sgcl::io::buffer sink;
    lzma::writer w(sink);
    ASSERT_TRUE(w.write(bytes(t)));
    ASSERT_TRUE(w.close());
    auto marked = buffer_text(sink);
    EXPECT_EQ(marked.substr(5, 8), std::string(8, '\xFF'));
    auto both = marked;
    for (int i = 0; i < 8; ++i) {
        both[5 + i] = char(uint64_t(t.size()) >> (8 * i));
    }
    for (auto& c : {known, marked, both}) {
        auto back = lzma::decompress(bytes(c));
        ASSERT_TRUE(back) << back.error().message();
        EXPECT_EQ(text(*back), t);
        for (size_t feed : {size_t(1), size_t(3), size_t(1) << 20}) {
            lzma::reader r(dribble{c, feed});
            std::string got;
            ASSERT_TRUE(read_everything(r, 4096, got)) << feed;
            EXPECT_EQ(got, t);
        }
#if SGCL_TEST_LIBLZMA
        std::string x;
        ASSERT_TRUE(xz_decode(c, x));
        EXPECT_EQ(x, t);
#endif
    }
    // a marker before the size known is corrupt
    auto early = marked;
    for (int i = 0; i < 8; ++i) {
        early[5 + i] = char(uint64_t(t.size() + 1) >> (8 * i));
    }
    auto bad = lzma::decompress(bytes(early));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), compress::errc::corrupt);
}

TEST(Lzma_Tests, EmptyAndOneByte) {
    for (std::string t : {std::string(), std::string("x")}) {
        auto c = lzma::compress(bytes(t));
        auto back = lzma::decompress(c);
        ASSERT_TRUE(back) << back.error().message();
        EXPECT_EQ(text(*back), t);
        sgcl::io::buffer sink;
        lzma::writer w(sink);
        if (!t.empty()) {
            ASSERT_TRUE(w.write(bytes(t)));
        }
        ASSERT_TRUE(w.close());
        auto s = buffer_text(sink);
        auto back2 = lzma::decompress(bytes(s));
        ASSERT_TRUE(back2) << back2.error().message();
        EXPECT_EQ(text(*back2), t);
#if SGCL_TEST_LIBLZMA
        std::string x;
        ASSERT_TRUE(xz_decode(text(c), x));
        EXPECT_EQ(x, t);
        ASSERT_TRUE(xz_decode(s, x));
        EXPECT_EQ(x, t);
#endif
    }
}

// lc, lp, pb and dictionaries of every kind, both ways with liblzma
TEST(Lzma_Tests, PropertiesAndDictionaries) {
    auto t = read_oracle("compress/gettysburg.txt") + read_oracle("compress/e.txt");
    struct Case { uint8_t lc, lp, pb; uint32_t dict; int level; };
    for (Case k : {Case{0, 0, 0, 4096, 6}, Case{8, 0, 2, 0, 6}, Case{3, 4, 4, 0, 6}, Case{4, 0, 0, 0, 1}, Case{0, 4, 4, 5000, 2}, Case{1, 2, 3, 65536 + 7, 9}, Case{8, 4, 4, 0, 5}}) {
        auto c = lzma::compress(bytes(t), {.level = k.level, .dictionary = k.dict, .lc = k.lc, .lp = k.lp, .pb = k.pb});
        EXPECT_EQ(uint8_t(c[0]), uint8_t((k.pb * 5 + k.lp) * 9 + k.lc));
        auto back = lzma::decompress(c);
        ASSERT_TRUE(back) << back.error().message();
        EXPECT_EQ(text(*back), t);
        sgcl::io::buffer sink;
        lzma::writer w(sink, {.level = k.level, .dictionary = k.dict, .lc = k.lc, .lp = k.lp, .pb = k.pb});
        ASSERT_TRUE(w.write(bytes(t)));
        ASSERT_TRUE(w.close());
        auto s = buffer_text(sink);
        auto back2 = lzma::decompress(bytes(s));
        ASSERT_TRUE(back2) << back2.error().message();
        EXPECT_EQ(text(*back2), t);
#if SGCL_TEST_LIBLZMA
        if (k.lc + k.lp <= 4) {   // liblzma's LZMA1 decoder takes lc + lp up to 4 only
            std::string x;
            ASSERT_TRUE(xz_decode(text(c), x)) << int(k.lc) << int(k.lp) << int(k.pb);
            EXPECT_EQ(x, t);
            ASSERT_TRUE(xz_decode(s, x));
            EXPECT_EQ(x, t);
        }
#endif
    }
}

TEST(Lzma_Tests, OptionsOutOfRangeAreTheProgramsMistake) {
    auto t = bytes(std::string("text"));
    EXPECT_THROW(lzma::compress(t, {.lc = 9}), std::invalid_argument);
    EXPECT_THROW(lzma::compress(t, {.lp = 5}), std::invalid_argument);
    EXPECT_THROW(lzma::compress(t, {.pb = 5}), std::invalid_argument);
    EXPECT_THROW(lzma::compress(t, {.dictionary = 4095}), std::invalid_argument);
    EXPECT_THROW(lzma::compress(t, {.dictionary = (uint32_t(3) << 29) + 1}), std::invalid_argument);
    EXPECT_THROW(lzma::compress(t, {.level = compress::level::huffman_only}), std::invalid_argument);
    sgcl::io::buffer sink;
    lzma::writer w(sink, {.lc = 9});
    auto r = w.write(t);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), sgcl::error_code(compress::errc::invalid_argument));
    EXPECT_FALSE(w.close());
    EXPECT_EQ(sink.size(), 0u);
}

// Cut short anywhere, or with bits flipped: an error or other bytes, never
// a crash (.lzma has no checksum: a flipped bit may decode to something)
TEST(Lzma_Tests, DamagedDataFails) {
    auto t = read_oracle("compress/gettysburg.txt");
    auto known = text(lzma::compress(bytes(t)));
    sgcl::io::buffer sink;
    lzma::writer w(sink);
    ASSERT_TRUE(w.write(bytes(t)));
    ASSERT_TRUE(w.close());
    auto marked = buffer_text(sink);
    for (auto& c : {known, marked}) {
        for (size_t n = 0; n < c.size(); ++n) {
            auto cut = c.substr(0, n);
            auto r = lzma::decompress(bytes(cut));
            ASSERT_FALSE(r) << n;
            auto code = r.error().code();
            EXPECT_TRUE(code == compress::errc::unexpected_end || code == compress::errc::corrupt) << n << ": " << r.error().message();
            lzma::reader rd(dribble{cut, 5});
            std::string got;
            EXPECT_FALSE(read_everything(rd, 100, got)) << n;
        }
    }
    std::mt19937 rng(3);
    int failed = 0;
    for (int i = 0; i < 3000; ++i) {
        auto c = i % 2 ? known : marked;
        size_t at = lzma::HeaderSize + rng() % (c.size() - lzma::HeaderSize);
        c[at] = char(c[at] ^ (1 << (rng() % 8)));
        auto r = lzma::decompress(bytes(c));
        if (!r) {
            ++failed;
            auto code = r.error().code();
            EXPECT_TRUE(code == compress::errc::unexpected_end || code == compress::errc::corrupt) << r.error().message();
        }
        lzma::reader rd(dribble{c, 64});
        std::string got;
        bool ok = read_everything(rd, 512, got);
        EXPECT_EQ(ok, bool(r));
        if (ok && r) {
            EXPECT_EQ(got, text(*r));
        }
    }
    EXPECT_GT(failed, 2000);
    auto bad = known;
    bad[0] = char(225);
    auto r = lzma::decompress(bytes(bad));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::invalid_header);
    bad = known;
    bad[lzma::HeaderSize] = 1;   // the range coder's first byte must be 0
    r = lzma::decompress(bytes(bad));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::corrupt);
}

// A dictionary past max_memory is too_large before anything is taken, in
// memory and as a stream; a large one over a small size known is taken as
// large as the data
TEST(Lzma_Tests, TheDictionaryIsHeldAgainstTheMemoryLimit) {
    auto t = read_oracle("compress/e.txt");
    sgcl::io::buffer sink;
    lzma::writer w(sink, {.dictionary = uint32_t(3) << 29});
    ASSERT_TRUE(w.write(bytes(t)));
    ASSERT_TRUE(w.close());
    auto c = buffer_text(sink);
    auto r = lzma::decompress(bytes(c));   // 1.5 GiB past the default 1 GiB
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    EXPECT_EQ(r.error().offset(), 0u);
    lzma::reader rd(dribble{c, 1000});
    auto n = rd.read_all();
    ASSERT_FALSE(n);
    ASSERT_TRUE(rd.last_error());
    EXPECT_EQ(rd.last_error()->code(), compress::errc::too_large);
    // a header asking for 4 GiB
    auto huge = c;
    for (int i = 1; i <= 4; ++i) {
        huge[i] = '\xFF';
    }
    r = lzma::decompress(bytes(huge));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    // under a lifted limit, the stream decodes
    auto ok = lzma::decompress(bytes(c), compress::limits{.max_memory = uint64_t(2) << 30});
    ASSERT_TRUE(ok) << ok.error().message();
    EXPECT_EQ(text(*ok), t);
    // the size known: the dictionary taken no larger than the data
    auto known = text(lzma::compress(bytes(t)));
    for (int i = 1; i <= 4; ++i) {
        known[i] = '\xFF';
    }
    auto small = lzma::decompress(bytes(known), compress::limits{.max_memory = uint64_t(1) << 20});
    ASSERT_TRUE(small) << small.error().message();
    EXPECT_EQ(text(*small), t);
    lzma::reader rs(dribble{known, 999}, compress::limits{.max_memory = uint64_t(1) << 20});
    std::string got;
    ASSERT_TRUE(read_everything(rs, 4096, got));
    EXPECT_EQ(got, t);
    auto tight = lzma::decompress(bytes(c), compress::limits{.max_memory = 4096});
    ASSERT_FALSE(tight);
    EXPECT_EQ(tight.error().code(), compress::errc::too_large);
}

TEST(Lzma_Tests, TheLimitStopsABomb) {
    std::string zeros(size_t(10) << 20, '\0');
    auto known = lzma::compress(bytes(zeros));
    EXPECT_LT(known.size(), 3000u);
    auto r = lzma::decompress(known, compress::limits{.max_size = 1 << 20});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    EXPECT_EQ(r.error().offset(), 0u);   // the size in the header, before any work
    sgcl::io::buffer sink;
    lzma::writer w(sink);
    ASSERT_TRUE(w.write(bytes(zeros)));
    ASSERT_TRUE(w.close());
    auto marked = buffer_text(sink);
    r = lzma::decompress(bytes(marked), compress::limits{.max_size = 1 << 20});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    auto exact = lzma::decompress(bytes(marked), compress::limits{.max_size = zeros.size()});
    ASSERT_TRUE(exact) << exact.error().message();
    EXPECT_EQ(exact->size(), zeros.size());
}

// A writer written in pieces of every size, a reader fed a byte, two,
// three and seven at a time; more than a fast level's window, so that the
// writer's window moves down and the reader's dictionary goes round
TEST(Lzma_Tests, StreamsInPiecesOfAnySize) {
    auto big = repo_text(size_t(3) << 20);
    auto t = read_oracle("compress/e.txt");
    struct Case { std::string data; int level; std::vector<size_t> steps; };
    for (auto& k : {Case{t, 6, {1, 7, 1000, t.size() + 1}}, Case{big, 0, {65536, big.size()}}, Case{big, 3, {1 << 20}}}) {
        for (size_t step : k.steps) {
            sgcl::io::buffer sink;
            lzma::writer w(sink, {.level = k.level});
            for (size_t i = 0; i < k.data.size(); i += step) {
                ASSERT_TRUE(w.write(bytes(k.data.substr(i, step))));
            }
            ASSERT_TRUE(w.close());
            ASSERT_TRUE(w.close());   // a second close does nothing
            auto c = buffer_text(sink);
#if SGCL_TEST_LIBLZMA
            std::string x;
            ASSERT_TRUE(xz_decode(c, x)) << "level " << k.level << " step " << step;
            ASSERT_EQ(x, k.data);
#endif
            for (size_t feed : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(1) << 20}) {
                if (k.data.size() > 100000 && feed < 1000) {
                    continue;
                }
                lzma::reader r(dribble{c, feed});
                std::string got;
                ASSERT_TRUE(read_everything(r, feed == 1 ? 1 : 3000, got)) << "feed " << feed << ": " << (r.last_error() ? r.last_error()->message() : sgcl::string(""));
                ASSERT_EQ(got, k.data) << "level " << k.level << " feed " << feed;
            }
        }
    }
}

TEST(Lzma_Tests, TheAsyncForms) {
    auto t = repo_text(size_t(400) << 10);
    auto task = sgcl::async::spawn([](std::string t) -> sgcl::async::task<std::string> {
        sgcl::io::buffer sink;
        lzma::writer w(sink, {.level = 5});
        for (size_t i = 0; i < t.size(); i += 100000) {
            if (!co_await w.async_write(bytes(t.substr(i, 100000)))) {
                co_return "write failed";
            }
        }
        if (!co_await w.async_close()) {
            co_return "close failed";
        }
        auto c = buffer_text(sink);
        lzma::reader r(dribble{c, 1000});
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(t));
    EXPECT_EQ(task.wait(), t);
    sgcl::async::scheduler::stop();
}

// The first failure of out kept: every write and close after it gives it
// at once, and nothing more is written to out
TEST(Lzma_Tests, AWriterKeepsItsFirstError) {
    std::mt19937 rng(9);
    std::string noise(1 << 20, 0);
    for (auto& ch : noise) {
        ch = char(rng());
    }
    failing_writer out(1000);
    lzma::writer w(out, {.level = 1});
    bool failed = false;
    for (size_t i = 0; i < noise.size(); i += 65536) {
        auto r = w.write(bytes(noise.substr(i, 65536)));
        if (!r) {
            failed = true;
            EXPECT_EQ(r.error().code(), sgcl::error_code(EIO, std::system_category()));
        }
    }
    ASSERT_TRUE(failed);
    int calls = out.calls;
    auto c = w.close();
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), sgcl::error_code(EIO, std::system_category()));
    ASSERT_TRUE(w.last_error());
    EXPECT_EQ(w.last_error()->code(), sgcl::error_code(EIO, std::system_category()));
    EXPECT_EQ(out.calls, calls);
    EXPECT_EQ(out.failures, 1);
    // the task's forms the same
    failing_writer out2(1000);
    auto task = sgcl::async::spawn([](failing_writer& o, std::string n) -> sgcl::async::task<int> {
        lzma::writer w(o, {.level = 1});
        for (size_t i = 0; i < n.size(); i += 65536) {
            (void)co_await w.async_write(bytes(n.substr(i, 65536)));
        }
        auto c = co_await w.async_close();
        co_return !c && c.error().code() == sgcl::error_code(EIO, std::system_category()) ? o.failures : -1;
    }(out2, noise));
    EXPECT_EQ(task.wait(), 1);
    sgcl::async::scheduler::stop();
}

TEST(Lzma_Tests, ResetReusesTheWriterAndTheReader) {
    sgcl::io::buffer a;
    sgcl::io::buffer b;
    auto one = read_oracle("compress/e.txt");
    lzma::writer w(a);
    ASSERT_TRUE(w.write(bytes(one)));
    ASSERT_TRUE(w.close());
    w.reset(b);
    ASSERT_TRUE(w.write(std::string("two")));
    ASSERT_TRUE(w.close());
    auto ca = buffer_text(a);
    auto cb = buffer_text(b);
    lzma::reader r(dribble{ca, 4000});
    EXPECT_EQ(text(value_of(r.read_all())), one);
    r.reset(dribble{cb, 4});
    EXPECT_EQ(text(value_of(r.read_all())), "two");
#if SGCL_TEST_LIBLZMA
    std::string x;
    ASSERT_TRUE(xz_decode(cb, x));
    EXPECT_EQ(x, "two");
#endif
}

// A reader failing with the compress category through an io handle
TEST(Lzma_Tests, AFailureReadThroughAHandle) {
    auto c = text(lzma::compress(bytes(read_oracle("compress/e.txt"))));
    c.resize(c.size() / 2);
    lzma::reader r(dribble{c, 100});
    sgcl::io::reader h(r);
    auto all = h.read_all();
    ASSERT_FALSE(all);
    EXPECT_EQ(all.error().code(), sgcl::error_code(compress::errc::unexpected_end));
    ASSERT_TRUE(r.last_error());
    EXPECT_GT(r.last_error()->offset(), 13u);
}
