//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "common.h"

using namespace compress_test;
using compress::flate;
using compress::gzip;
using compress::zlib;

// Every level, zlib inflating what we make, and we inflating what zlib
// makes with each of its strategies, as data in memory
TEST(Flate_Tests, BothWaysWithZlibAtEveryLevel) {
    for (auto& [name, t] : corpus()) {
        for (int level : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, compress::level::huffman_only}) {
            auto c = flate::compress(bytes(t), {.level = level});
            std::string back;
            ASSERT_TRUE(z_inflate(text(c), -15, back)) << name << " level " << level;
            ASSERT_EQ(back, t) << name << " level " << level;
        }
        for (int level : {0, 1, 6, 9}) {
            for (int strategy : {Z_DEFAULT_STRATEGY, Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED}) {
                auto z = z_deflate(t, level, -15, strategy);
                auto back = flate::decompress(bytes(z));
                ASSERT_TRUE(back) << name << " level " << level << " strategy " << strategy << ": " << back.error().message();
                ASSERT_EQ(text(*back), t) << name;
            }
        }
    }
}

// A stored block between coded ones, reached from the fast loop: the
// bytes the loop's refill read ahead must not be ORed into the header of
// the block after the stored one (text, then bytes that do not compress,
// then text again; zlib makes stored blocks only at level 0, so this is
// our encoder's stream, read by us and by zlib)
TEST(Flate_Tests, AStoredBlockBetweenCodedOnes) {
    std::mt19937 rng(5);
    std::string noise(20000, 0);
    for (auto& c : noise) {
        c = char(rng());
    }
    auto words = read_oracle("compress/e.txt").substr(0, 20000);
    auto t = words + noise + words + noise + words;
    for (int level : {1, 2, 3, 4, 5, 6, 7, 8, 9}) {
        auto c = text(flate::compress(bytes(t), {.level = level}));
        std::string z;
        ASSERT_TRUE(z_inflate(c, -15, z)) << level;
        ASSERT_EQ(z, t);
        auto ours = flate::decompress(bytes(c));
        ASSERT_TRUE(ours) << "level " << level << ": " << ours.error().message();
        ASSERT_EQ(text(*ours), t) << level;
        for (size_t feed : {size_t(1), size_t(4096), size_t(1) << 20}) {
            flate::reader r(dribble{c, feed});
            auto all = r.read_all();
            ASSERT_TRUE(all) << level << " feed " << feed;
            ASSERT_EQ(text(*all), t) << level << " feed " << feed;
        }
    }
}

// Go's golden files: blocks of its encoder with no final block after
// them, so the stream ends short; everything before the end decodes to
// the input, and the end is unexpected_end
TEST(Flate_Tests, GoGoldenFilesDecode) {
    for (auto base : {"huffman-null-max", "huffman-pi", "huffman-rand-1k", "huffman-rand-limit", "huffman-shifts", "huffman-text", "huffman-text-shift", "huffman-zero"}) {
        auto in = read_oracle(std::string("flate/") + base + ".in");
        for (auto kind : {".dyn.expect", ".wb.expect"}) {
            auto golden = read_oracle(std::string("flate/") + base + kind);
            if (golden.empty()) {
                continue;
            }
            flate::reader r(dribble{golden, 1 << 20});
            std::string got;
            std::byte buf[4096];
            for (;;) {
                auto n = r.read(buf);
                if (!n || *n == 0) {
                    break;
                }
                got.append(reinterpret_cast<const char*>(buf), *n);
            }
            ASSERT_TRUE(r.last_error()) << base << kind;
            EXPECT_EQ(r.last_error()->code(), compress::errc::unexpected_end) << base << kind << ": " << r.last_error()->message();
            EXPECT_EQ(got, in) << base << kind;
        }
    }
}

// The streams: a writer written in pieces of every size, with a flush in
// the middle, and a reader fed a byte, two, three and seven at a time
TEST(Flate_Tests, StreamsInPiecesOfAnySize) {
    for (auto& [name, t] : corpus()) {
        if (t.size() > 70000) {
            continue;
        }
        for (size_t step : {size_t(1), size_t(7), size_t(1000), t.size() + 1}) {
            sgcl::io::buffer sink;
            flate::writer w(sink, {.level = 6});
            for (size_t i = 0; i < t.size(); i += step) {
                ASSERT_TRUE(w.write(bytes(t.substr(i, step))));
                if (i == t.size() / 2) {
                    ASSERT_TRUE(w.flush());
                }
            }
            ASSERT_TRUE(w.close());
            std::string c(reinterpret_cast<const char*>(sink.data().data()), sink.size());
            std::string back;
            ASSERT_TRUE(z_inflate(c, -15, back)) << name << " step " << step;
            ASSERT_EQ(back, t);
            for (size_t feed : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(1) << 20}) {
                flate::reader r(dribble{c, feed});
                auto all = r.read_all();
                ASSERT_TRUE(all) << name << " feed " << feed;
                ASSERT_EQ(text(*all), t) << name << " feed " << feed;
            }
        }
    }
}

TEST(Flate_Tests, ADictionaryBothWays) {
    std::string dict = "the quick brown fox jumps over the lazy dog";
    std::string t = "the lazy dog jumps over the quick brown fox, the quick brown fox";
    auto c = flate::compress(bytes(t), {.level = 9, .dictionary = bytes(dict)});
    auto plain = flate::compress(bytes(t), {.level = 9});
    EXPECT_LT(c.size(), plain.size());
    auto back = flate::decompress(bytes(text(c)), {.dictionary = bytes(dict)});
    ASSERT_TRUE(back) << back.error().message();
    EXPECT_EQ(text(*back), t);
    // zlib's raw inflate with the same dictionary
    z_stream d{};
    inflateInit2(&d, -15);
    inflateSetDictionary(&d, (const Bytef*)dict.data(), uInt(dict.size()));
    std::string out(1000, 0);
    std::string in = text(c);
    d.next_in = (Bytef*)in.data();
    d.avail_in = uInt(in.size());
    d.next_out = (Bytef*)out.data();
    d.avail_out = uInt(out.size());
    EXPECT_EQ(inflate(&d, Z_FINISH), Z_STREAM_END);
    out.resize(d.total_out);
    inflateEnd(&d);
    EXPECT_EQ(out, t);
    flate::reader r(dribble{in, 3}, {.dictionary = bytes(dict)});
    EXPECT_EQ(text(value_of(r.read_all())), t);
}

// A dictionary of any size up to the window, and past it (its last 32 KB
// count), in memory and through the reader
TEST(Flate_Tests, DictionariesOfEverySize) {
    auto t = read_oracle("compress/e.txt").substr(0, 5000);
    for (size_t size : {size_t(1), size_t(500), size_t(2000), size_t(8000), size_t(32768), size_t(40000)}) {
        std::string dict = read_oracle("compress/pi.txt").substr(0, size);
        auto c = text(flate::compress(bytes(t), {.dictionary = bytes(dict)}));
        auto back = flate::decompress(bytes(c), {.dictionary = bytes(dict)});
        ASSERT_TRUE(back) << size << ": " << back.error().message();
        EXPECT_EQ(text(*back), t) << size;
        auto z = text(zlib::compress(bytes(t), {.dictionary = bytes(dict)}));
        auto zback = zlib::decompress(bytes(z), {.dictionary = bytes(dict)});
        ASSERT_TRUE(zback) << size << ": " << zback.error().message();
        EXPECT_EQ(text(*zback), t) << size;
        flate::reader r(dribble{c, 7}, {.dictionary = bytes(dict)});
        EXPECT_EQ(text(value_of(r.read_all())), t) << size;
        // the limit counts the output, not the dictionary
        EXPECT_TRUE(flate::decompress(bytes(c), {.dictionary = bytes(dict)}, compress::limits{t.size()}));
        EXPECT_FALSE(flate::decompress(bytes(c), {.dictionary = bytes(dict)}, compress::limits{t.size() - 1}));
    }
}

// A gzip length that promises more than the data can make is a hint, not
// an allocation (a 23-byte member saying 4 GiB)
TEST(Gzip_Tests, ALengthInTheTrailerIsOnlyAHint) {
    auto c = text(gzip::compress(bytes(std::string("hello"))));
    c[c.size() - 4] = c[c.size() - 3] = c[c.size() - 2] = c[c.size() - 1] = char(0xFF);
    auto r = gzip::decompress(bytes(c));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::corrupt);   // the length does not match; nothing large was made
}

// A writer reset keeps its dictionary: the second stream is the one a new
// writer with that dictionary makes
TEST(Flate_Tests, ResetKeepsTheDictionary) {
    std::string dict = "alpha beta gamma delta epsilon";
    std::string t = "gamma delta alpha beta epsilon gamma";
    sgcl::io::buffer a;
    sgcl::io::buffer b;
    flate::writer w(a, {.dictionary = bytes(dict)});
    ASSERT_TRUE(w.write(bytes(t)));
    ASSERT_TRUE(w.close());
    w.reset(b);
    ASSERT_TRUE(w.write(bytes(t)));
    ASSERT_TRUE(w.close());
    std::string ca(reinterpret_cast<const char*>(a.data().data()), a.size());
    std::string cb(reinterpret_cast<const char*>(b.data().data()), b.size());
    EXPECT_EQ(ca, cb);
    EXPECT_EQ(text(value_of(flate::decompress(bytes(cb), {.dictionary = bytes(dict)}))), t);
}

TEST(Flate_Tests, CorruptAndTruncatedDataFail) {
    auto t = read_oracle("compress/gettysburg.txt");
    auto c = text(flate::compress(bytes(t)));
    auto cut = flate::decompress(bytes(c.substr(0, c.size() / 2)));
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), compress::errc::unexpected_end);
    auto type3 = flate::decompress(bytes(std::string("\x07", 1)));
    ASSERT_FALSE(type3);
    EXPECT_EQ(type3.error().code(), compress::errc::corrupt);
    // a stored block whose length does not match its complement
    auto stored = flate::decompress(bytes(std::string("\x01\x05\x00\x00\x00hello", 10)));
    ASSERT_FALSE(stored);
    EXPECT_EQ(stored.error().code(), compress::errc::corrupt);
    // a distance before the start: a fixed block whose first symbol is a match
    auto early = flate::decompress(bytes(z_deflate("aaaaaaaa", 9, -15, Z_FIXED).substr(0, 0) + std::string("\x03\x02", 2)));
    EXPECT_FALSE(early);
    // every single bit flipped in a small stream: an error or bytes, never a crash (ASan)
    auto small = text(flate::compress(bytes(std::string("hello hello hello world"))));
    for (size_t bit = 0; bit < small.size() * 8; ++bit) {
        auto flipped = small;
        flipped[bit / 8] = char(flipped[bit / 8] ^ (1 << (bit % 8)));
        (void)flate::decompress(bytes(flipped));
    }
    flate::reader r(dribble{c.substr(0, c.size() / 2), 5});
    auto partial = r.read_all();
    ASSERT_FALSE(partial);
    ASSERT_TRUE(r.last_error());
    EXPECT_EQ(r.last_error()->code(), compress::errc::unexpected_end);
}

TEST(Flate_Tests, TheLimitStopsABomb) {
    std::string zeros(10 << 20, '\0');
    auto c = flate::compress(bytes(zeros), {.level = 9});
    EXPECT_LT(c.size(), 20000u);
    auto limited = flate::decompress(bytes(text(c)), compress::limits{1 << 20});
    ASSERT_FALSE(limited);
    EXPECT_EQ(limited.error().code(), compress::errc::too_large);
    auto whole = flate::decompress(bytes(text(c)), compress::limits{UINT64_MAX});
    ASSERT_TRUE(whole);
    EXPECT_EQ(whole->size(), zeros.size());
}

// A sync flush after every piece, at every level: each flush ends a block
// whose codes are built for the few symbols it has (one literal and the
// end; a block with no match, whose distance table has no symbol; one
// with a single distance; the fixed codes chosen for the smallest), a
// flush with nothing new after it, and records of one shape (the HTTP
// middleware's live JSON in 16 pieces); zlib inflates every stream
TEST(Flate_Tests, AFlushAfterEveryPiece) {
    std::string json = "[";
    for (size_t i = 0; json.size() < 65536; ++i) {
        json += "{\"id\":" + std::to_string(i) + ",\"name\":\"item " + std::to_string(i) + "\",\"ok\":true},";
    }
    json.resize(65536);
    std::vector<std::pair<std::string, std::string>> inputs = {{"json", json}, {"run", std::string(5000, 'a')}, {"one", "x"}};
    for (auto& [name, t] : corpus()) {
        if (name == "compress/gettysburg.txt" || name == "random" || name == "len32769") {
            inputs.push_back({name, t.substr(0, 40000)});
        }
    }
    for (auto& [name, t] : inputs) {
        for (int level : {0, 1, 3, 4, 5, 6, 7, 9, compress::level::huffman_only}) {
            for (size_t step : {size_t(1), size_t(3), size_t(37), size_t(4096)}) {
                if (step < 37 && t.size() > 6000) {
                    continue;   // a block a byte: the small inputs alone
                }
                sgcl::io::buffer sink;
                flate::writer w(sink, {.level = level});
                for (size_t i = 0; i < t.size(); i += step) {
                    ASSERT_TRUE(w.write(bytes(t.substr(i, step))));
                    ASSERT_TRUE(w.flush());
                    if (i == 0) {
                        ASSERT_TRUE(w.flush());   // nothing new: an empty stored block alone
                    }
                }
                ASSERT_TRUE(w.close());
                std::string c(reinterpret_cast<const char*>(sink.data().data()), sink.size());
                ASSERT_GE(c.size(), 5u) << name;
                std::string back;
                ASSERT_TRUE(z_inflate(c, -15, back)) << name << " level " << level << " step " << step;
                ASSERT_EQ(back, t) << name << " level " << level << " step " << step;
                auto ours = flate::decompress(bytes(c));
                ASSERT_TRUE(ours) << name;
                ASSERT_EQ(text(*ours), t) << name;
            }
        }
    }
}

// A flush makes what came before it decodable there: a reader gets it
// all before any byte after the flush is written
TEST(Flate_Tests, AFlushIsDecodableWhereItIs) {
    sgcl::io::buffer sink;
    flate::writer w(sink);
    ASSERT_TRUE(w.write(std::string("before the flush")));
    ASSERT_TRUE(w.flush());
    std::string part(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    flate::reader r(dribble{part, 3});
    std::byte buf[64];
    std::string got;
    for (;;) {
        auto n = r.read(buf);
        if (!n || *n == 0) break;
        got.append(reinterpret_cast<const char*>(buf), *n);
    }
    EXPECT_EQ(got, "before the flush");
    EXPECT_FALSE(w.is_closed());
    ASSERT_TRUE(w.close());
    EXPECT_TRUE(w.is_closed());
    EXPECT_FALSE(w.write(std::string("after")));
}

TEST(Flate_Tests, ALevelOutOfRangeIsTheProgramsMistake) {
    EXPECT_THROW(compress::level(10), std::invalid_argument);
    EXPECT_THROW(compress::level(-1), std::invalid_argument);
    EXPECT_EQ(compress::level().value(), 6);
    EXPECT_EQ(compress::level(compress::level::huffman_only).value(), -2);
}

TEST(Gzip_Tests, TheLimitStopsABomb) {
    std::string zeros(10 << 20, '\0');
    auto c = gzip::compress(bytes(zeros), {.level = 9});
    auto limited = gzip::decompress(bytes(text(c)), compress::limits{1 << 20});
    ASSERT_FALSE(limited);
    EXPECT_EQ(limited.error().code(), compress::errc::too_large);
}

TEST(Zlib_Tests, BothWaysAndTheChecksum) {
    for (auto& [name, t] : corpus()) {
        auto c = zlib::compress(bytes(t));
        std::string back;
        ASSERT_TRUE(z_inflate(text(c), 15, back)) << name;
        ASSERT_EQ(back, t);
        auto z = z_deflate(t, 6, 15);
        auto ours = zlib::decompress(bytes(z));
        ASSERT_TRUE(ours) << name << ": " << ours.error().message();
        ASSERT_EQ(text(*ours), t);
        zlib::reader r(dribble{z, 3});
        ASSERT_EQ(text(value_of(r.read_all())), t) << name;
    }
    auto z = z_deflate("some data", 6, 15);
    z[z.size() - 1] = char(z[z.size() - 1] ^ 1);
    auto bad = zlib::decompress(bytes(z));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), compress::errc::checksum);
}

TEST(Zlib_Tests, APresetDictionaryIsNamedAndRequired) {
    std::string dict = "common words: alpha beta gamma delta";
    std::string t = "alpha beta gamma delta alpha";
    auto c = zlib::compress(bytes(t), {.dictionary = bytes(dict)});
    uLong id = adler32(adler32(0, nullptr, 0), (const Bytef*)dict.data(), uInt(dict.size()));
    ASSERT_TRUE(zlib::dictionary_id(bytes(text(c))));
    EXPECT_EQ(*zlib::dictionary_id(bytes(text(c))), uint32_t(id));
    auto without = zlib::decompress(bytes(text(c)));
    ASSERT_FALSE(without);
    EXPECT_EQ(without.error().code(), compress::errc::dictionary_required);
    auto with = zlib::decompress(bytes(text(c)), {.dictionary = bytes(dict)});
    ASSERT_TRUE(with) << with.error().message();
    EXPECT_EQ(text(*with), t);
    // zlib inflates it with the dictionary when asked
    z_stream d{};
    inflateInit(&d);
    std::string in = text(c), out(200, 0);
    d.next_in = (Bytef*)in.data();
    d.avail_in = uInt(in.size());
    d.next_out = (Bytef*)out.data();
    d.avail_out = uInt(out.size());
    EXPECT_EQ(inflate(&d, Z_FINISH), Z_NEED_DICT);
    inflateSetDictionary(&d, (const Bytef*)dict.data(), uInt(dict.size()));
    EXPECT_EQ(inflate(&d, Z_FINISH), Z_STREAM_END);
    out.resize(d.total_out);
    inflateEnd(&d);
    EXPECT_EQ(out, t);
    zlib::reader r(dribble{in, 2});
    auto no = r.read_all();
    EXPECT_FALSE(no);
    EXPECT_EQ(r.dictionary_id(), uint32_t(id));
}

TEST(Gzip_Tests, BothWaysWithEveryMember) {
    for (auto& [name, t] : corpus()) {
        auto c = gzip::compress(bytes(t));
        std::string back;
        ASSERT_TRUE(z_inflate(text(c), 31, back)) << name;
        ASSERT_EQ(back, t);
        auto z = z_deflate(t, 6, 31);
        auto ours = gzip::decompress(bytes(z));
        ASSERT_TRUE(ours) << name << ": " << ours.error().message();
        ASSERT_EQ(text(*ours), t);
    }
    // two members, one stream; single_member stops after the first
    auto both = text(gzip::compress(bytes(std::string("first ")))) + text(gzip::compress(bytes(std::string("second"))));
    EXPECT_EQ(text(value_of(gzip::decompress(bytes(both)))), "first second");
    gzip::reader all(dribble{both, 1});
    EXPECT_EQ(text(value_of(all.read_all())), "first second");
    // members whose next header is in the buffer already: a source that
    // gives everything at once, and a member of 100 000 bytes among them
    std::string big(100000, 'x');
    auto four = text(gzip::compress(bytes(std::string("first ")))) + text(gzip::compress(bytes(std::string("second"))))
              + text(gzip::compress(bytes(big))) + text(gzip::compress(bytes(std::string("tail"))));
    for (size_t feed : {size_t(1) << 20, size_t(4096), size_t(1)}) {
        gzip::reader r4(dribble{four, feed});
        EXPECT_EQ(text(value_of(r4.read_all())), "first second" + big + "tail") << feed;
    }
    gzip::reader one(dribble{both, 1}, gzip::single_member);
    EXPECT_EQ(text(value_of(one.read_all())), "first ");
}

TEST(Gzip_Tests, TheHeaderInLatin1BothWays) {
    gzip::header h;
    h.name = "r\xC3\xA9sum\xC3\xA9.txt";   // résumé.txt
    h.comment = "caf\xC3\xA9";
    h.modified = sgcl::time::datetime::from_unix(1700000000, sgcl::time::zone::utc());
    h.extra = sgcl::vector<std::byte>{std::byte(1), std::byte(2), std::byte(3)};
    h.os = 3;
    sgcl::io::buffer sink;
    gzip::writer w(sink, {.header = h});
    ASSERT_TRUE(w.write(std::string("body")));
    ASSERT_TRUE(w.close());
    std::string c(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    // the name is ISO 8859-1 in the file: é is one byte, 0xE9
    EXPECT_NE(c.find("r\xE9sum\xE9.txt", 0, 11), std::string::npos);
    gzip::reader r(dribble{c, 3});
    auto got = r.header();
    ASSERT_TRUE(got) << got.error().message();
    EXPECT_EQ(got->name, h.name);
    EXPECT_EQ(got->comment, h.comment);
    ASSERT_TRUE(got->modified);
    EXPECT_EQ(got->modified->unix(), 1700000000);
    EXPECT_EQ(got->extra.size(), 3u);
    EXPECT_EQ(got->os, 3);
    EXPECT_EQ(text(value_of(r.read_all())), "body");
}

// A name or a comment past ISO 8859-1 is written as its UTF-8 bytes, as
// gzip(1) writes a file's name, and so is one whose ISO 8859-1 bytes would
// read back as UTF-8 (Ã© is C3 A9, the UTF-8 of é); read, bytes that are
// UTF-8 are taken as they are and others as ISO 8859-1: every text a
// program writes reads back the same
TEST(Gzip_Tests, TextPastLatin1IsWrittenAsUtf8) {
    struct Case {
        std::string text;
        std::string written;
    };
    for (const Case& c : {Case{"\xE2\x82\xAC.txt", "\xE2\x82\xAC.txt"},                 // €.txt
                          Case{"\xE6\x97\xA5\xE6\x9C\xAC", "\xE6\x97\xA5\xE6\x9C\xAC"},   // 日本
                          Case{"\xC3\x83\xC2\xA9", "\xC3\x83\xC2\xA9"},                     // Ã©
                          Case{"r\xC3\xA9sum\xC3\xA9", "r\xE9sum\xE9"},                       // résumé
                          Case{"plain", "plain"}}) {
        gzip::header h;
        h.name = c.text;
        h.comment = c.text;
        auto packed = gzip::compress(bytes(std::string("body")), {.header = h});
        std::string raw(reinterpret_cast<const char*>(packed.data()), packed.size());
        std::string field = c.written + std::string(1, '\0');
        EXPECT_EQ(raw.substr(10, field.size()), field) << c.text;
        EXPECT_EQ(raw.substr(10 + field.size(), field.size()), field) << c.text;
        gzip::reader r(dribble{raw, 2});
        auto got = r.header();
        ASSERT_TRUE(got) << got.error().message();
        EXPECT_EQ(std::string(got->name.view()), c.text);
        EXPECT_EQ(std::string(got->comment.view()), c.text);
        EXPECT_EQ(text(value_of(r.read_all())), "body");
    }
    // the headers of other writers: gzip(1)'s UTF-8, Go's ISO 8859-1, and
    // bytes that are neither, read as ISO 8859-1
    for (const auto& [name, read] : {std::pair<std::string, std::string>{"\xC3\xA9", "\xC3\xA9"},
                                     {"\xE9", "\xC3\xA9"},
                                     {"\xFF\xFE", "\xC3\xBF\xC3\xBE"},
                                     {"\xE2\x82", "\xC3\xA2\xC2\x82"}}) {
        std::string member("\x1F\x8B\x08\x08\0\0\0\0\0\xFF", 10);
        member += name + std::string(1, '\0');
        member += std::string("\x03\x00", 2);   // an empty final block
        member += std::string(8, '\0');          // the CRC-32 and the length of nothing
        gzip::reader r(dribble{member, 3});
        auto got = r.header();
        ASSERT_TRUE(got) << got.error().message();
        EXPECT_EQ(std::string(got->name.view()), read);
        EXPECT_EQ(text(value_of(r.read_all())), "");
    }
}

// What cannot throw is declared so: gzip's compress with the default
// header, and the readers' async_close, which return the task of in's
static_assert(noexcept(gzip::compress(std::declval<sgcl::slice<const std::byte>>())));
static_assert(noexcept(gzip::compress(std::declval<sgcl::string>())));
static_assert(noexcept(gzip::compress("text")));
static_assert(!noexcept(gzip::compress("text", gzip::options{})));
static_assert(noexcept(std::declval<flate::reader&>().async_close()));
static_assert(noexcept(std::declval<zlib::reader&>().async_close()));
static_assert(noexcept(std::declval<gzip::reader&>().async_close()));
static_assert(noexcept(std::declval<compress::bzip2::reader&>().async_close()));
static_assert(noexcept(std::declval<compress::lzw::reader&>().async_close()));
static_assert(noexcept(std::declval<compress::lzma::reader&>().async_close()));
static_assert(noexcept(std::declval<compress::xz::reader&>().async_close()));
static_assert(noexcept(std::declval<compress::tar::reader&>().async_close()));

// A header gzip cannot write is refused, never a stream without it
TEST(Gzip_Tests, AHeaderItCannotWriteIsRefused) {
    gzip::header nul;
    nul.name = sgcl::string(std::string_view("a\0b", 3));
    EXPECT_THROW(gzip::compress(bytes(std::string("hello")), {.header = nul}), std::invalid_argument);
    gzip::header comment_nul;
    comment_nul.comment = sgcl::string(std::string_view("c\0d", 3));
    EXPECT_THROW(gzip::compress(bytes(std::string("hello")), {.header = comment_nul}), std::invalid_argument);
    gzip::header extra;
    extra.extra.resize(70000);
    EXPECT_THROW(gzip::compress(bytes(std::string("hello")), {.header = extra}), std::invalid_argument);
    sgcl::io::buffer sink;
    gzip::writer w(sink, {.header = nul});
    auto e = w.write(std::string("x"));
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), compress::errc::invalid_argument);
    // the error says what is wrong with the header, the close gives it too
    EXPECT_EQ(std::string(e.error().message().view()), "write gzip: a name with a NUL: invalid argument");
    auto c = w.close();
    ASSERT_FALSE(c);
    EXPECT_EQ(std::string(c.error().message().view()), "write gzip: a name with a NUL: invalid argument");
    gzip::writer quiet(sink, {.header = comment_nul});
    auto closed = quiet.close();   // nothing written: the close finds it
    ASSERT_FALSE(closed);
    EXPECT_EQ(std::string(closed.error().message().view()), "close gzip: a comment with a NUL: invalid argument");
    try {
        (void)gzip::compress(bytes(std::string("hello")), {.header = extra});
    } catch (const std::invalid_argument& x) {
        EXPECT_STREQ(x.what(), "compress::gzip: an extra field longer than 65535 bytes");
    }
}

TEST(Gzip_Tests, AFlippedCrcAndATruncatedTrailerFail) {
    auto c = text(gzip::compress(bytes(std::string("hello, gzip"))));
    auto crc = c;
    crc[crc.size() - 8] = char(crc[crc.size() - 8] ^ 0x40);
    auto bad = gzip::decompress(bytes(crc));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), compress::errc::checksum);
    auto cut = gzip::decompress(bytes(c.substr(0, c.size() - 3)));
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), compress::errc::unexpected_end);
    auto notgz = gzip::decompress(bytes(std::string("PK\x03\x04 not gzip")));
    ASSERT_FALSE(notgz);
    EXPECT_EQ(notgz.error().code(), compress::errc::invalid_header);
}

// The task's forms: a writer and a reader on the scheduler
TEST(Gzip_Tests, TheAsyncForms) {
    auto t = read_oracle("compress/e.txt");
    auto task = sgcl::async::spawn([](std::string t) -> sgcl::async::task<std::string> {
        sgcl::io::buffer sink;
        gzip::writer w(sink);
        (void)co_await w.async_write(bytes(t));
        (void)co_await w.async_close();
        std::string c(reinterpret_cast<const char*>(sink.data().data()), sink.size());
        gzip::reader r(dribble{c, 1000});
        auto h = co_await r.async_header();
        if (!h) {
            co_return "header failed";
        }
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(t));
    EXPECT_EQ(task.wait(), t);
    sgcl::async::scheduler::stop();
}

// The first failure of out is kept, by the three writers alike: every
// write, flush and close after it gives it at once and writes nothing;
// last_error() holds it
TEST(Gzip_Tests, AFailingSinkIsKeptToTheClose) {
    std::string data(200000, 0);
    std::mt19937 rng(3);
    for (auto& c : data) {
        c = char(rng());   // incompressible: out is written before the close
    }
    auto check = [&](auto& w, failing_after& f, const char* name) {
        EXPECT_FALSE(w.last_error()) << name;
        auto first = w.write(bytes(data));
        ASSERT_FALSE(first) << name;
        EXPECT_TRUE(is_eio(first.error())) << name;
        int calls = f.calls;
        auto more = w.write(std::string("more"));
        ASSERT_FALSE(more) << name;
        EXPECT_TRUE(is_eio(more.error())) << name;
        auto flushed = w.flush();
        ASSERT_FALSE(flushed) << name;
        EXPECT_TRUE(is_eio(flushed.error())) << name;
        auto closed = w.close();
        ASSERT_FALSE(closed) << name;
        EXPECT_TRUE(is_eio(closed.error())) << name;
        auto second = w.close();
        ASSERT_FALSE(second) << name;
        EXPECT_TRUE(is_eio(second.error())) << name;
        EXPECT_EQ(f.calls, calls) << name;   // nothing tried after the failure
        ASSERT_TRUE(w.last_error()) << name;
        EXPECT_TRUE(is_eio(*w.last_error())) << name;
    };
    failing_after f1(100), f2(100), f3(100);
    gzip::writer g(f1);
    check(g, f1, "gzip");
    zlib::writer z(f2);
    check(z, f2, "zlib");
    flate::writer d(f3);
    check(d, f3, "flate");
    // the task's forms
    auto t = sgcl::async::spawn([](std::string data) -> sgcl::async::task<std::string> {
        failing_after f(100);
        gzip::writer w(f);
        auto first = co_await w.async_write(bytes(data));
        if (first || !is_eio(first.error())) co_return "write";
        int calls = f.calls;
        auto more = co_await w.async_write(std::string("more"));
        if (more || !is_eio(more.error())) co_return "write after";
        auto flushed = co_await w.async_flush();
        if (flushed || !is_eio(flushed.error())) co_return "flush after";
        auto closed = co_await w.async_close();
        if (closed || !is_eio(closed.error())) co_return "close";
        co_return f.calls == calls ? "ok" : "written after";
    }(data));
    EXPECT_EQ(t.wait(), "ok");
    sgcl::async::scheduler::stop();
}

// A write or a flush after close is the caller's error, kept as a failure
// of out is: every close after it gives it
TEST(Gzip_Tests, AWriteAfterCloseIsKept) {
    for (int kind = 0; kind < 3; ++kind) {
        sgcl::io::buffer sink;
        auto check = [&](auto& w) {
            ASSERT_TRUE(w.write(std::string("data")));
            ASSERT_TRUE(w.close());
            ASSERT_TRUE(w.close());   // a second close does nothing
            EXPECT_FALSE(w.last_error());
            size_t size = sink.size();
            sgcl::optional<sgcl::io::error> first;   // zlib's first is a flush, the others' a write
            if (kind == 1) {
                auto r = w.flush();
                ASSERT_FALSE(r);
                first = r.error();
            } else {
                auto r = w.write(std::string("x"));
                ASSERT_FALSE(r);
                first = r.error();
            }
            auto& late = *first;
            EXPECT_TRUE(late.is_closed()) << kind;
            auto more = w.write(std::string("y"));
            ASSERT_FALSE(more) << kind;
            EXPECT_TRUE(more.error() == late) << kind;
            auto flushed = w.flush();
            ASSERT_FALSE(flushed) << kind;
            EXPECT_TRUE(flushed.error() == late) << kind;
            auto closed = w.close();
            ASSERT_FALSE(closed) << kind;
            EXPECT_TRUE(closed.error() == late) << kind;
            ASSERT_TRUE(w.last_error()) << kind;
            EXPECT_TRUE(*w.last_error() == late) << kind;
            EXPECT_EQ(sink.size(), size) << kind;
        };
        if (kind == 0) {
            gzip::writer w(sink);
            check(w);
        } else if (kind == 1) {
            zlib::writer w(sink);
            check(w);
        } else {
            flate::writer w(sink);
            check(w);
        }
    }
}

// A writer and a reader of the module (whose write and read are
// overloaded by io's mixins) given to io's handles by reference
TEST(Gzip_Tests, TheStreamsGoIntoIoHandlesByReference) {
    sgcl::io::buffer sink;
    gzip::writer w(sink);
    sgcl::io::writer h(w);
    ASSERT_TRUE(h.write(std::string("through the handle")));
    ASSERT_TRUE(w.close());
    std::string c(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    gzip::reader r(dribble{c, 5});
    sgcl::io::reader rh(r);
    EXPECT_EQ(text(value_of(rh.read_all())), "through the handle");
}

TEST(Gzip_Tests, ResetReusesTheWriterAndTheReader) {
    sgcl::io::buffer a;
    sgcl::io::buffer b;
    gzip::writer w(a);
    ASSERT_TRUE(w.write(std::string("one")));
    ASSERT_TRUE(w.close());
    w.reset(b);
    ASSERT_TRUE(w.write(std::string("two")));
    ASSERT_TRUE(w.close());
    std::string ca(reinterpret_cast<const char*>(a.data().data()), a.size());
    std::string cb(reinterpret_cast<const char*>(b.data().data()), b.size());
    gzip::reader r(dribble{ca, 4});
    EXPECT_EQ(text(value_of(r.read_all())), "one");
    r.reset(dribble{cb, 4});
    EXPECT_EQ(text(value_of(r.read_all())), "two");
}

namespace {
    // Rows of an image as PNG filters them: a smooth picture with a little
    // noise, 400 × 150 RGB, each row's bytes less the prediction of Sub, Up
    // or Paeth in turn (a filter byte first): data of small differences,
    // what the filtered strategy is for
    std::string filtered_rows() {
        const size_t w = 400 * 3, h = 150;
        std::mt19937 rng(3);
        std::vector<uint8_t> img(w * h);
        for (size_t y = 0; y < h; ++y) {
            for (size_t x = 0; x < w; ++x) {
                img[y * w + x] = uint8_t(128 + 60 * std::sin(double(x) / 90 + double(y) / 40) + (x % 3) * 20 + (rng() & 7));
            }
        }
        auto paeth = [](int a, int b, int c) {
            int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
            return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
        };
        std::string out;
        for (size_t y = 0; y < h; ++y) {
            const int f = 1 + int(y % 3) + (y % 3 == 2 ? 1 : 0);   // 1 Sub, 2 Up, 4 Paeth
            out += char(f);
            for (size_t x = 0; x < w; ++x) {
                const int cur = img[y * w + x];
                const int a = x >= 3 ? img[y * w + x - 3] : 0;
                const int b = y ? img[(y - 1) * w + x] : 0;
                const int c = x >= 3 && y ? img[(y - 1) * w + x - 3] : 0;
                const int pred = f == 1 ? a : f == 2 ? b : paeth(a, b, c);
                out += char(uint8_t(cur - pred));
            }
        }
        return out;
    }

    std::string deflate_with(const std::string& t, int level, bool filtered) {
        compress::detail::Deflater d(level, filtered);
        std::vector<uint8_t> out;
        d.write(reinterpret_cast<const uint8_t*>(t.data()), t.size(), out);
        d.finish(out);
        return std::string(out.begin(), out.end());
    }
}

// The filtered strategy (zlib's Z_FILTERED, what PNG's encoder asks for):
// at the chain levels (7 to 9) no match of 5 bytes or fewer, and the output
// no more than 1 % past zlib's with Z_FILTERED on the same data; the table
// levels (1 to 6) unchanged by it; zlib inflating all of it
TEST(Flate_Tests, TheFilteredStrategyAsZlibs) {
    auto sources = corpus();
    sources.push_back({"filtered rows", filtered_rows()});
    for (auto& [name, t] : sources) {
        for (int level : {1, 2, 3, 4, 5, 6, 7, 8, 9}) {
            const std::string ours = deflate_with(t, level, true);
            std::string back;
            ASSERT_TRUE(z_inflate(ours, -15, back)) << name << " level " << level;
            ASSERT_EQ(back, t) << name << " level " << level;
            auto ours_back = flate::decompress(bytes(ours));
            ASSERT_TRUE(ours_back) << name;
            ASSERT_EQ(text(*ours_back), t) << name;
            if (level <= 6) {
                EXPECT_EQ(ours, deflate_with(t, level, false)) << name << " level " << level;
                continue;
            }
            const size_t zlib_size = z_deflate(t, level, -15, Z_FILTERED).size();
            EXPECT_LE(double(ours.size()), double(zlib_size) * 1.01 + 16) << name << " level " << level << ": ours " << ours.size() << ", zlib " << zlib_size;
        }
    }
    // on filtered data it is what makes the difference: smaller than the
    // default strategy at the lazy levels
    const std::string rows = filtered_rows();
    for (int level : {7, 9}) {
        EXPECT_LT(deflate_with(rows, level, true).size(), deflate_with(rows, level, false).size()) << level;
    }
}

// The levels in order: from 1 to 9 the output does not grow (the table
// encoder of 1 to 6, the chains of 7 to 9, a step at most 1 % the wrong
// way where a corpus favours the table's longer hashes), and the chain
// levels are no more than 1 % past zlib's of the same level (their tuning
// is zlib's), on text, PNG's filtered rows, and binary data
TEST(Flate_Tests, TheLevelsInOrder) {
    std::mt19937 rng(11);
    std::string binary;
    for (int i = 0; i < 4000; ++i) {
        // records of a little-endian layout: small integers, a pointer-like
        // word, a tag from a few, and some noise
        uint32_t a = uint32_t(i), b = uint32_t(rng() % 100);
        uint64_t ptr = 0x00007f0000000000ull + uint64_t(rng() % 4096) * 64;
        binary.append(reinterpret_cast<const char*>(&a), 4);
        binary.append(reinterpret_cast<const char*>(&b), 4);
        binary.append(reinterpret_cast<const char*>(&ptr), 8);
        binary += "TAG" + std::to_string(rng() % 7);
        binary += char(rng());
    }
    std::vector<std::pair<std::string, std::string>> sources = {
        {"e.txt", read_oracle("compress/e.txt")},
        {"huffman-text", read_oracle("flate/huffman-text.in")},
        {"filtered rows", filtered_rows()},
        {"binary", binary},
    };
    for (auto& [name, t] : sources) {
        size_t previous = SIZE_MAX;
        for (int level = 1; level <= 9; ++level) {
            const size_t n = flate::compress(bytes(t), {.level = level}).size();
            EXPECT_LE(double(n), double(previous) * 1.01) << name << " level " << level << " is larger than level " << level - 1;
            previous = n;
            if (level >= 7) {
                const size_t z = z_deflate(t, level, -15, Z_DEFAULT_STRATEGY).size();
                EXPECT_LE(double(n), double(z) * 1.01 + 16) << name << " level " << level << ": ours " << n << ", zlib " << z;
            }
        }
    }
}

// The positions in the tables are absolute and brought back to 0 past a
// bound (2^31 in use, lowered here to 1 MB so a test reaches it): streams
// longer than the bound, and a thousand resets of one encoder (each moving
// the positions a window on, never clearing the tables), all decoding to
// their input, at a table level and a chain level
TEST(Flate_Tests, ThePositionsPastTheRebase) {
    struct Lowered {
        uint32_t was = compress::detail::Deflater::rebase_at;
        Lowered() {
            compress::detail::Deflater::rebase_at = uint32_t(1) << 20;
        }
        ~Lowered() {
            compress::detail::Deflater::rebase_at = was;
        }
    } lowered;
    std::string t;
    const std::string words = read_oracle("compress/gettysburg.txt");
    std::mt19937 rng(13);
    while (t.size() < (size_t(5) << 20)) {
        t += words.substr(rng() % words.size() / 2, 200 + rng() % 300);
    }
    for (int level : {1, 6, 7, 9}) {
        std::string back;
        const std::string c = deflate_with(t, level, false);
        ASSERT_TRUE(z_inflate(c, -15, back)) << level;
        ASSERT_EQ(back, t) << level;
        compress::detail::Deflater d(level);
        for (int i = 0; i < 1000; ++i) {
            const std::string piece = t.substr(size_t(i) * 997 % (t.size() - 3000), 1000 + size_t(i) % 2000);
            std::vector<uint8_t> out;
            d.write(reinterpret_cast<const uint8_t*>(piece.data()), piece.size(), out);
            d.finish(out);
            ASSERT_TRUE(z_inflate(std::string(out.begin(), out.end()), -15, back)) << level << " reset " << i;
            ASSERT_EQ(back, piece) << level << " reset " << i;
            d.reset();
        }
    }
}

// The one-shot compress uses a Deflater kept by the thread, reset to each
// call's level without clearing its tables: on one thread, levels in a row
// (tables growing, shrinking and coming back), with and without a
// dictionary, small inputs and ones past the window, its output is byte
// for byte that of a Deflater made for the call, and decodes. Again with
// the rebase lowered to 256 KB, so that the kept positions reach it: a
// stream that could reach it starts from cleared tables, as a new one
// would (the rebase within a stream moves the chains' ring)
TEST(Flate_Tests, TheThreadsDeflaterAcrossLevels) {
    std::string t;
    const std::string words = read_oracle("compress/gettysburg.txt");
    std::mt19937 rng(17);
    while (t.size() < 300000) {
        t += words.substr(rng() % words.size() / 2, 200 + rng() % 300);
    }
    auto fresh = [](int level, const std::string& in, const std::string& dict) {
        compress::detail::Deflater d(level, reinterpret_cast<const uint8_t*>(dict.data()), dict.size());
        std::vector<uint8_t> out;
        d.write(reinterpret_cast<const uint8_t*>(in.data()), in.size(), out);
        d.finish(out);
        return std::string(out.begin(), out.end());
    };
    const int levels[] = {6, 9, 1, 6, 0, compress::level::huffman_only, 7, 3, 9, 8, 2, 5, 4, 6, 0, 9};
    const size_t sizes[] = {0, 16, 1024, 70000, 200000};
    for (uint32_t bound : {uint32_t(1) << 31, uint32_t(1) << 18}) {
        const uint32_t was = compress::detail::Deflater::rebase_at;
        compress::detail::Deflater::rebase_at = bound;
        for (int with_dictionary = 0; with_dictionary < 2; ++with_dictionary) {
            for (int level : levels) {
                for (size_t size : sizes) {
                    const std::string in = t.substr(rng() % 1000, size);
                    const std::string dict = with_dictionary ? t.substr(rng() % 50000, 1000 + rng() % 40000) : std::string();
                    const std::string got = text(flate::compress(bytes(in), {.level = level, .dictionary = bytes(dict)}));
                    ASSERT_EQ(got, fresh(level, in, dict)) << bound << " level " << level << " size " << size << " dictionary " << dict.size();
                    std::string back = text(flate::decompress(bytes(got), {.dictionary = bytes(dict)}).value());
                    ASSERT_EQ(back, in) << bound << " level " << level << " size " << size;
                    const auto g = gzip::compress(bytes(in), {.level = level});
                    ASSERT_EQ(text(gzip::decompress(bytes(text(g))).value()), in) << bound << " gzip level " << level << " size " << size;
                }
            }
        }
        compress::detail::Deflater::rebase_at = was;
    }
}

// A compress made while the thread's Deflater is out (one inside another)
// gets a Deflater of its own; the output is the same either way, and the
// thread's is lent again once given back
TEST(Flate_Tests, ACompressWhileTheThreadsDeflaterIsOut) {
    const std::string dict = read_oracle("compress/gettysburg.txt");
    const std::string t = dict.substr(50, 1200) + dict.substr(0, 700);
    auto fresh = [&](int level) {
        compress::detail::Deflater d(level);
        std::vector<uint8_t> out;
        d.write(reinterpret_cast<const uint8_t*>(t.data()), t.size(), out);
        d.finish(out);
        return std::string(out.begin(), out.end());
    };
    {
        compress::detail::LentDeflater outer(9, nullptr, 0, t.size());
        EXPECT_TRUE(outer.kept());
        {
            compress::detail::LentDeflater inner(1, nullptr, 0, t.size());
            EXPECT_FALSE(inner.kept());
            EXPECT_EQ(text(flate::compress(bytes(t), {.level = 6})), fresh(6));
            std::vector<uint8_t> out;
            inner->write(reinterpret_cast<const uint8_t*>(t.data()), t.size(), out);
            inner->finish(out);
            EXPECT_EQ(std::string(out.begin(), out.end()), fresh(1));
        }
        std::vector<uint8_t> out;
        outer->write(reinterpret_cast<const uint8_t*>(t.data()), t.size(), out);
        outer->finish(out);
        EXPECT_EQ(std::string(out.begin(), out.end()), fresh(9));
    }
    compress::detail::LentDeflater again(6, nullptr, 0, t.size());
    EXPECT_TRUE(again.kept());
}

// Huffman only with a dictionary: the history is kept for nothing (no
// tables to seed), and the stream decodes as any other
TEST(Flate_Tests, HuffmanOnlyWithADictionary) {
    const std::string dict = read_oracle("compress/gettysburg.txt");
    const std::string t = dict.substr(100, 900) + "and more text after it";
    compress::detail::Deflater d(compress::detail::Deflater::HuffmanOnly, reinterpret_cast<const uint8_t*>(dict.data()), dict.size());
    std::vector<uint8_t> out;
    d.write(reinterpret_cast<const uint8_t*>(t.data()), t.size(), out);
    d.finish(out);
    std::string back;
    ASSERT_TRUE(z_inflate(std::string(out.begin(), out.end()), -15, back));
    EXPECT_EQ(back, t);
}
