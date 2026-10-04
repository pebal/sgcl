//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Reading Deflate64 (7z's 040109, zip's method 9). A stream made here by
// hand with the fixed codes takes what 7-Zip's encoder never writes — a
// length of 65 538, distances past 49 153 and to 64 KB back — through the
// decoder whole, through a stream in pieces of a few bytes, and through a
// 7z folder; 7-Zip (7zz) makes archives of both formats at three levels of
// files whose matches reach past 32 KB, which we read byte for byte; cut
// and flipped archives fail and never crash. Without 7zz those tests are
// skipped.
#include "common.h"
#include "tests/source_root.h"

#include <cstdio>
#include <set>

using namespace compress_test;
namespace sevenzip = sgcl::compress::sevenzip;
namespace zip = sgcl::compress::zip;
namespace fs = std::filesystem;
namespace cd = sgcl::compress::detail;

namespace {
    // DEFLATE's bits: least significant first; a Huffman code most significant first
    class BitWriter {
    public:
        void bits(uint32_t v, unsigned n) {
            for (unsigned i = 0; i < n; ++i) {
                bit((v >> i) & 1);
            }
        }

        void code(uint32_t c, unsigned n) {
            for (unsigned i = n; i-- > 0;) {
                bit((c >> i) & 1);
            }
        }

        // a symbol of the fixed literal/length code (RFC 1951 3.2.6)
        void litlen(unsigned s) {
            if (s < 144) {
                code(0x30 + s, 8);
            } else if (s < 256) {
                code(0x190 + s - 144, 9);
            } else if (s < 280) {
                code(s - 256, 7);
            } else {
                code(0xC0 + s - 280, 8);
            }
        }

        std::string done() {
            if (_n) {
                _out += char(_byte);
            }
            return _out;
        }

    private:
        void bit(uint32_t b) {
            _byte |= uint8_t(b << _n);
            if (++_n == 8) {
                _out += char(_byte);
                _byte = 0;
                _n = 0;
            }
        }

        std::string _out;
        uint8_t _byte = 0;
        unsigned _n = 0;
    };

    struct Match {
        uint32_t length, distance;
    };

    // A Deflate64 stream of one fixed block: literals, then matches that
    // only Deflate64 has; and the bytes it decodes to
    std::pair<std::string, std::string> hand_made() {
        std::mt19937 rng(9);
        std::string plain;
        BitWriter w;
        w.bits(1, 1);   // the last block
        w.bits(1, 2);   // the fixed codes
        for (int i = 0; i < 70000; ++i) {
            unsigned c = rng() & 0xFF;
            w.litlen(c);
            plain += char(c);
        }
        std::vector<Match> matches = {
            {65538, 65536},   // 285 with 16 extra bits at their top; distance code 31 at its top
            {1000, 40000},    // distance code 30
            {258, 49153},     // code 31 at its base; 258 as 285 (base 3, extra 255)
            {3, 32769},       // code 30 at its base, the shortest length through 285
            {300, 1},         // a run longer than DEFLATE's longest match
            {20000, 7},       // a short period, long
            {40, 30000},      // DEFLATE's own codes still (28 and 29 map to lengths/distances as ever)
        };
        auto distance = [&](uint32_t d) {
            if (d > 32768) {
                unsigned c = d >= 49153 ? 31 : 30;
                w.code(c, 5);
                w.bits(d - (c == 31 ? 49153 : 32769), 14);
                return;
            }
            for (unsigned c = 29;; --c) {
                if (d >= cd::DistanceBase[c]) {
                    w.code(c, 5);
                    w.bits(d - cd::DistanceBase[c], cd::DistanceExtra[c]);
                    return;
                }
            }
        };
        for (auto m : matches) {
            if (m.length > 130 || m.length == 3 && m.distance == 32769) {
                w.litlen(285);
                w.bits(m.length - 3, 16);
            } else {
                unsigned c = 27;
                while (cd::LengthBase[c] > m.length) {
                    --c;
                }
                w.litlen(257 + c);
                w.bits(m.length - cd::LengthBase[c], cd::LengthExtra[c]);
            }
            distance(m.distance);
            for (uint32_t i = 0; i < m.length; ++i) {
                plain += plain[plain.size() - m.distance];
            }
        }
        w.litlen(256);
        return {w.done(), plain};
    }

    std::string text_of(const sgcl::vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // A 7z archive of one Deflate64 folder of our stream
    std::string seven_zip_of(const std::string& packed, const std::string& plain) {
        namespace wr = cd::sevenzip_writing;
        wr::FolderRecord f;
        wr::CoderRecord c;
        c.method = cd::sevenzip_format::Deflate64;
        f.coders.push_back(c);
        f.unpacked = plain.size();
        f.packed = packed.size();
        f.sizes.push_back(plain.size());
        f.crcs.push_back(sgcl::hash::crc32::of(compress_test::bytes(plain)));
        wr::FileRecord r;
        r.name = u"data.bin";
        r.stream = true;
        r.size = plain.size();
        r.crc = f.crcs[0];
        r.attributes = 0x8020 | (0100644u << 16);
        std::vector<uint8_t> out(32, 0);
        out.insert(out.end(), packed.begin(), packed.end());
        auto h = wr::header({f}, {r});
        std::vector<uint8_t> hp, he;
        wr::packed_header(h, packed.size(), hp, he);
        out.insert(out.end(), hp.begin(), hp.end());
        out.insert(out.end(), he.begin(), he.end());
        auto sig = wr::signature(packed.size() + hp.size(), he);
        std::copy(sig.begin(), sig.end(), out.begin());
        return std::string(out.begin(), out.end());
    }

    void put(const fs::path& p, const std::string& data) {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary).write(data.data(), std::streamsize(data.size()));
    }

    std::string repo_text(size_t limit) {
        auto root = source_root() / "sgcl";
        std::vector<fs::path> files;
        for (auto& e : fs::recursive_directory_iterator(root)) {
            if (e.path().extension() == ".h") {
                files.push_back(e.path());
            }
        }
        std::sort(files.begin(), files.end());
        std::string all;
        for (auto& f : files) {
            all += slurp(f);
            if (all.size() >= limit) {
                break;
            }
        }
        all.resize(std::min(all.size(), limit));
        return all;
    }

    // Files whose matches reach past 32 KB and run long: the repository's
    // headers, a random block of 40 KB four times over, a block of 50 KB
    // twice with a change between, a long run
    fs::path make_tree(const fs::path& dir) {
        auto t = dir / "tree";
        std::mt19937 rng(64);
        std::string block(40000, 0), wide(50000, 0);
        for (auto& c : block) c = char(rng());
        for (auto& c : wide) c = char(rng());
        put(t / "text.txt", repo_text(1500000));
        put(t / "repeated.bin", block + block + block + block);
        put(t / "far.bin", wide + "change" + wide + std::string(3000, 'x') + wide);
        put(t / "run.bin", std::string(300000, '\0') + "end");
        put(t / "small.txt", "Deflate64\n");
        return t;
    }

#define SKIP_WITHOUT_7ZZ() \
    if (seven_zip().empty()) GTEST_SKIP() << "7zz not found"
}

// Our stream whole in memory, then DEFLATE's decoder refusing it (the
// distance codes 30 and 31 are not DEFLATE's)
TEST(Deflate64_Tests, AHandMadeStreamWhole) {
    auto [packed, plain] = hand_made();
    auto s = std::make_unique<cd::InflateState>();
    s->reset();
    std::vector<uint8_t> out(plain.size() + cd::MaxMatch + 64);
    const uint8_t* in = reinterpret_cast<const uint8_t*>(packed.data());
    size_t pos = 0;
    auto st = cd::inflate64(*s, in, in + packed.size(), out.data(), pos, out.size());
    ASSERT_EQ(st, cd::InflateStatus::done) << (s->error_text ? s->error_text : "");
    ASSERT_EQ(pos, plain.size());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(out.data()), pos), plain);
    EXPECT_EQ(in, reinterpret_cast<const uint8_t*>(packed.data()) + packed.size());
    s->reset();
    in = reinterpret_cast<const uint8_t*>(packed.data());
    pos = 0;
    st = cd::inflate(*s, in, in + packed.size(), out.data(), pos, out.size());
    EXPECT_EQ(st, cd::InflateStatus::failed);
    // the output exactly as large: the long matches into the last bytes
    s->reset();
    std::vector<uint8_t> tight(plain.size());
    in = reinterpret_cast<const uint8_t*>(packed.data());
    pos = 0;
    st = cd::inflate64(*s, in, in + packed.size(), tight.data(), pos, tight.size());
    ASSERT_EQ(st, cd::InflateStatus::done) << (s->error_text ? s->error_text : "");
    ASSERT_EQ(pos, plain.size());
    EXPECT_TRUE(std::string(reinterpret_cast<const char*>(tight.data()), pos) == plain);
}

// Through a stream (the zip reader's): the input a few bytes a read, the
// output in reads of odd sizes, the window slid under the long matches
TEST(Deflate64_Tests, AHandMadeStreamInPieces) {
    auto [packed, plain] = hand_made();
    for (size_t step : {1u, 3u, 7u, 4096u, 1u << 20}) {
        for (size_t take : {1u, 333u, 70000u}) {
            sgcl::compress::flate::reader r(dribble{packed, step});
            cd::use_deflate64(r);
            std::string got;
            std::vector<std::byte> buf(take);
            for (;;) {
                auto n = r.read(sgcl::slice<std::byte>(buf.data(), buf.size()));
                ASSERT_TRUE(n) << step << " " << take << ": " << n.error().message();
                if (*n == 0) {
                    break;
                }
                got.append(reinterpret_cast<const char*>(buf.data()), *n);
            }
            ASSERT_EQ(got.size(), plain.size()) << step << " " << take;
            EXPECT_TRUE(got == plain) << step << " " << take;
        }
    }
    // cut anywhere: an error, never a crash or a short read taken as the end
    for (size_t cut = 0; cut < packed.size(); cut += packed.size() / 97 + 1) {
        sgcl::compress::flate::reader r(dribble{packed.substr(0, cut), 5});
        cd::use_deflate64(r);
        auto all = r.read_all();
        EXPECT_FALSE(all) << cut;
    }
}

// Through a 7z folder of method 040109, and 7-Zip reads it as we do
TEST(Deflate64_Tests, AHandMadeFolder) {
    auto [packed, plain] = hand_made();
    auto archive = seven_zip_of(packed, plain);
    auto a = sevenzip::archive::from(compress_test::bytes(archive));
    ASSERT_TRUE(a) << a.error().message();
    auto d = a->read("data.bin");
    ASSERT_TRUE(d) << d.error().message();
    EXPECT_TRUE(text_of(*d) == plain);
    size_t walked = 0;
    for (auto [e, r] : a->walk()) {
        auto all = r.read_all();
        ASSERT_TRUE(all);
        EXPECT_TRUE(text_of(*all) == plain);
        ++walked;
    }
    EXPECT_EQ(walked, 1u);
    if (!seven_zip().empty()) {
        scratch_dir scratch("sgcl-d64");
        put(scratch.path() / "hand.7z", archive);
        ASSERT_EQ(run_in(scratch.path(), seven_zip() + " x hand.7z"), 0);
        EXPECT_TRUE(slurp(scratch.path() / "data.bin") == plain);
    }
}

// 7-Zip's Deflate64 at three levels, in 7z (solid and not) and in zip: read
// byte for byte, by read() and by the walk
TEST(Deflate64_Tests, SevenZipArchivesRead) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-d64");
    auto tree = make_tree(scratch.path());
    std::vector<std::string> names = {"text.txt", "repeated.bin", "far.bin", "run.bin", "small.txt"};
    for (int level : {1, 5, 9}) {
        for (std::string solid : {"on", "off"}) {
            std::string what = "7z mx" + std::to_string(level) + " solid " + solid;
            auto path = scratch.path() / ("d64-" + std::to_string(level) + solid + ".7z");
            ASSERT_EQ(run_in(tree, seven_zip() + " a -m0=Deflate64 -mx=" + std::to_string(level) + " -ms=" + solid + " '" + path.string() + "' ."), 0) << what;
            auto a = sevenzip::archive::open(sgcl::string(path.string()));
            ASSERT_TRUE(a) << what << ": " << a.error().message();
            for (auto& n : names) {
                auto d = a->read(sgcl::string(n));
                ASSERT_TRUE(d) << what << " " << n << ": " << d.error().message();
                EXPECT_TRUE(text_of(*d) == slurp(tree / n)) << what << " " << n;
            }
            for (auto [e, r] : a->walk()) {
                auto all = r.read_all();
                ASSERT_TRUE(all) << what << " walk " << e.name;
                EXPECT_TRUE(text_of(*all) == slurp(tree / std::string(e.name.view()))) << what << " walk " << e.name;
            }
        }
        std::string what = "zip mx" + std::to_string(level);
        auto path = scratch.path() / ("d64-" + std::to_string(level) + ".zip");
        ASSERT_EQ(run_in(tree, seven_zip() + " a -tzip -mm=Deflate64 -mx=" + std::to_string(level) + " '" + path.string() + "' ."), 0) << what;
        auto z = zip::archive::open(sgcl::string(path.string()));
        ASSERT_TRUE(z) << what << ": " << z.error().message();
        size_t seen = 0;
        for (auto& e : z->entries()) {
            if (e.is_directory()) {
                continue;
            }
            if (e.size > 1000) {
                EXPECT_EQ(e.method, zip::method::deflate64) << what << " " << e.name;   // 7-Zip stores the smallest
            }
            auto d = z->read(e);
            ASSERT_TRUE(d) << what << " " << e.name << ": " << d.error().message();
            EXPECT_TRUE(text_of(*d) == slurp(tree / std::string(e.name.view()))) << what << " " << e.name;
            // and through the entry's reader, a few bytes a read
            auto r = z->reader(e);
            ASSERT_TRUE(r);
            std::string got;
            std::vector<std::byte> buf(1000);
            for (;;) {
                auto n = r->read(sgcl::slice<std::byte>(buf.data(), buf.size()));
                ASSERT_TRUE(n) << what << " " << e.name;
                if (*n == 0) {
                    break;
                }
                got.append(reinterpret_cast<const char*>(buf.data()), *n);
            }
            EXPECT_TRUE(got == slurp(tree / std::string(e.name.view()))) << what << " reader " << e.name;
            ++seen;
        }
        EXPECT_EQ(seen, names.size()) << what;
    }
}

// Cut and flipped: every cut archive and every flipped bit of the data
// fails (a CRC-32 or the decoder), none crashes; zip's flips inside its
// entries' data, 7z's anywhere but the minor version (not covered by a CRC)
TEST(Deflate64_Tests, DamageIsFound) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-d64");
    auto t = scratch.path() / "tree";
    std::mt19937 rng(5);
    std::string block(36000, 0);
    for (auto& c : block) c = char(rng());
    put(t / "a.bin", block + block + repo_text(60000) + block);
    put(t / "b.txt", repo_text(30000));
    ASSERT_EQ(run_in(t, seven_zip() + " a -m0=Deflate64 ../d.7z ."), 0);
    ASSERT_EQ(run_in(t, seven_zip() + " a -tzip -mm=Deflate64 ../d.zip ."), 0);
    auto sz = slurp(scratch.path() / "d.7z");
    auto zp = slurp(scratch.path() / "d.zip");
    auto read_all_7z = [](const std::string& data) {
        auto a = sevenzip::archive::from(compress_test::bytes(data));
        if (!a) {
            return false;
        }
        for (auto& e : a->entries()) {
            if (!a->read(e)) {
                return false;
            }
        }
        return true;
    };
    auto read_all_zip = [](const std::string& data) {
        auto a = zip::archive::from(compress_test::bytes(data));
        if (!a) {
            return false;
        }
        for (auto& e : a->entries()) {
            if (!a->read(e)) {
                return false;
            }
        }
        return true;
    };
    ASSERT_TRUE(read_all_7z(sz));
    ASSERT_TRUE(read_all_zip(zp));
    for (size_t cut = 0; cut < sz.size(); cut += sz.size() / 61 + 1) {
        EXPECT_FALSE(read_all_7z(sz.substr(0, cut))) << "7z cut " << cut;
    }
    size_t flips = 0;
    for (size_t at = 0; at < sz.size(); at += sz.size() / 400 + 1) {
        if (at == 7) {
            continue;
        }
        auto bad = sz;
        bad[at] = char(bad[at] ^ (1 << (at % 8)));
        EXPECT_FALSE(read_all_7z(bad)) << "7z flip at " << at;
        ++flips;
    }
    EXPECT_GT(flips, 300u);
    // zip: flips inside every entry's compressed data
    auto z = zip::archive::from(compress_test::bytes(zp));
    ASSERT_TRUE(z);
    for (auto& e : z->entries()) {
        if (e.is_directory()) {
            continue;
        }
        size_t name = uint8_t(zp[e.offset + 26]) | uint8_t(zp[e.offset + 27]) << 8;
        size_t extra = uint8_t(zp[e.offset + 28]) | uint8_t(zp[e.offset + 29]) << 8;
        size_t start = size_t(e.offset) + 30 + name + extra;
        for (size_t k = 0; k < e.compressed_size; k += e.compressed_size / 150 + 1) {
            auto bad = zp;
            bad[start + k] = char(bad[start + k] ^ (1 << (k % 8)));
            auto a = zip::archive::from(compress_test::bytes(bad));
            ASSERT_TRUE(a);
            EXPECT_FALSE(a->read(e)) << "zip " << e.name << " flip at " << k;
        }
        for (size_t cut = 0; cut < e.compressed_size; cut += e.compressed_size / 40 + 1) {
            sgcl::compress::flate::reader r(dribble{zp.substr(start, cut), 4096});
            cd::use_deflate64(r);
            EXPECT_FALSE(r.read_all()) << "zip " << e.name << " cut " << cut;
        }
    }
}

// Read, not written: zip's writer refuses method 9 (7z's method has no value for it)
TEST(Deflate64_Tests, ZipWriterRefusesIt) {
    sgcl::io::buffer b;
    zip::writer w(b);
    zip::entry e;
    e.name = "x.bin";
    e.method = zip::method::deflate64;
    auto c = w.create(e);
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), sgcl::compress::errc::unsupported);
}
