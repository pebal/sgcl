//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Reading 7z. The oracles make the archives: 7-Zip itself (7zz) with every
// method, filter and switch that matters — solid and not, packed and plain
// headers, PPMd of other orders and memories, BCJ2, the branch converters,
// passwords, anti-items — and libarchive (bsdtar) with the methods it
// writes; each archive is of a tree the test makes (text, a program,
// random bytes, an empty file, an empty directory, a symbolic link, a name
// past ASCII), and what we read is compared with the tree. Without the
// tools the tests are skipped. Files go in a directory of their own under
// the system's temporary directory, removed when the test ends.
#include "common.h"

#include <sys/stat.h>

#include <cstdio>
#include <map>
#include <set>
#include <thread>

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif

using namespace compress_test;
namespace sevenzip = sgcl::compress::sevenzip;
namespace fs = std::filesystem;

namespace {
    std::string repo_text(size_t limit) {
        auto root = fs::path(__FILE__).parent_path().parent_path().parent_path() / "sgcl";
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

    std::string program(size_t limit) {
        for (auto f : {"/bin/zsh", "/bin/bash", "/bin/ls"}) {
            auto s = slurp(f);
            if (s.size() > 50000) {
                s.resize(std::min(s.size(), limit));
                return s;
            }
        }
        return std::string(limit, 'p');
    }

    void put(const fs::path& p, const std::string& data) {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary).write(data.data(), std::streamsize(data.size()));
    }

    // The tree the archives are made of, under dir/tree
    fs::path make_tree(const fs::path& dir) {
        auto t = dir / "tree";
        std::mt19937 rng(3);
        std::string noise(60000, 0);
        for (auto& c : noise) {
            c = char(rng());
        }
        put(t / "text.txt", repo_text(250000));
        put(t / "dir/prog.bin", program(150000));
        put(t / "dir/random.bin", noise);
        put(t / "empty.txt", "");
        put(t / "dir/sub/one.txt", "x");
        put(t / "dir/n\xC3\xAFme \xE2\x82\xAC.txt", "na\xC3\xAFve \xF0\x9F\x98\x80");
        fs::create_directories(t / "empty-dir");
        fs::create_symlink("dir/sub/one.txt", t / "link");
        return t;
    }

    std::string text_of(const sgcl::vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // Every entry of the archive against the tree: the set of names, the
    // kinds, the data (by read() and by walk()), the times and modes
    void check_against_tree(const sevenzip::archive& a, const fs::path& tree, const std::string& what, bool times = true) {
        std::set<std::string> expected;
        for (auto& e : fs::recursive_directory_iterator(tree)) {
            expected.insert(e.path().lexically_relative(tree).generic_string());
        }
        std::set<std::string> seen;
        for (auto& e : a.entries()) {
            std::string name(e.name.view());
            if (name == "." || name == "./") {
                continue;   // bsdtar's entry of the root
            }
            if (name.rfind("./", 0) == 0) {
                name = name.substr(2);
            }
            if (!name.empty() && name.back() == '/') {
                name.pop_back();
            }
            seen.insert(name);
            fs::path p = tree / name;
            EXPECT_TRUE(e.is_local()) << what << " " << name;
            EXPECT_FALSE(e.is_anti) << what << " " << name;
            auto data = a.read(e);
            ASSERT_TRUE(data) << what << " " << name << ": " << data.error().message();
            if (fs::is_symlink(p)) {
                EXPECT_TRUE(e.is_symlink()) << what << " " << name;
                EXPECT_EQ(text_of(*data), fs::read_symlink(p).string()) << what << " " << name;
            } else if (fs::is_directory(p)) {
                EXPECT_TRUE(e.is_directory) << what << " " << name;
                EXPECT_EQ(e.size, 0u);
            } else {
                EXPECT_FALSE(e.is_directory) << what << " " << name;
                ASSERT_EQ(text_of(*data), slurp(p)) << what << " " << name;
                if (e.attributes & 0x8000) {
                    struct stat st;
                    ASSERT_EQ(::lstat(p.c_str(), &st), 0);
                    EXPECT_EQ((e.attributes >> 16) & 07777, st.st_mode & 07777) << what << " " << name;
                }
            }
            if (times && !fs::is_symlink(p)) {
                ASSERT_TRUE(e.modified) << what << " " << name;
                auto ft = fs::last_write_time(p);
                auto sys = std::chrono::file_clock::to_sys(ft);
                int64_t secs = std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count();
                EXPECT_LE(std::abs(e.modified->unix() - secs), 1) << what << " " << name;
            }
        }
        EXPECT_EQ(seen, expected) << what;
        // one pass with one decoder a folder gives the same bytes
        size_t walked = 0;
        for (auto [e, r] : a.walk()) {
            auto all = r.read_all();
            ASSERT_TRUE(all) << what << " walk " << e.name;
            auto again = a.read(e);
            ASSERT_TRUE(again);
            EXPECT_EQ(text_of(*all), text_of(*again)) << what << " walk " << e.name;
            ++walked;
        }
        EXPECT_EQ(walked, a.entries().size());
    }

#define SKIP_WITHOUT_7ZZ() \
    if (seven_zip().empty()) GTEST_SKIP() << "7zz not found"
}

// 7-Zip's archives of every method and filter, solid and not, the header
// packed and plain; PPMd of three orders and memories; BCJ2 (four streams
// and a range coder, bound to three LZMA coders)
TEST(SevenZip_Tests, Every7ZipMethod) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7z");
    auto tree = make_tree(scratch.path());
    std::vector<std::pair<std::string, std::string>> cases = {
        {"lzma", "-m0=LZMA"}, {"lzma2", "-m0=LZMA2"}, {"ppmd", "-m0=PPMd"}, {"bzip2", "-m0=BZip2"}, {"deflate", "-m0=Deflate"},
        {"copy", "-m0=Copy"}, {"nonsolid", "-ms=off"}, {"plainheader", "-mhc=off"}, {"ppmd2", "-m0=PPMd:o=2:mem=1m"},
        {"ppmd32", "-m0=PPMd:o=32:mem=24m"}, {"ppmd16", "-m0=PPMd:o=16:mem=2m"}, {"bcj2", "-mf=BCJ2 -m0=LZMA"},
        {"x86", "-mf=BCJ"}, {"arm64", "-mf=ARM64"}, {"arm", "-mf=ARM"}, {"armt", "-mf=ARMT"}, {"ppc", "-mf=PPC"},
        {"sparc", "-mf=SPARC"}, {"ia64", "-mf=IA64"}, {"riscv", "-mf=RISCV"}, {"delta", "-mf=Delta:4"},
        {"lzma-bcj", "-m0=BCJ -m1=LZMA:d=1m"}, {"level1", "-mx=1"}, {"level9", "-mx=9"},
    };
    for (auto& [name, switches] : cases) {
        auto archive = scratch.path() / (name + ".7z");
        ASSERT_EQ(run_in(tree, seven_zip() + " a -snl " + switches + " '" + archive.string() + "' ."), 0) << name;
        auto a = sevenzip::archive::open(sgcl::string(archive.string()));
        ASSERT_TRUE(a) << name << ": " << a.error().message();
        check_against_tree(*a, tree, name);
        (void)a->close();
    }
}

// libarchive's archives of the methods it writes
TEST(SevenZip_Tests, EveryLibarchiveMethod) {
    if (!have_bsdtar()) {
        GTEST_SKIP() << "bsdtar not found";
    }
    scratch_dir scratch("sgcl-7z");
    auto tree = make_tree(scratch.path());
    for (std::string m : {"lzma1", "lzma2", "ppmd", "bzip2", "deflate", "copy"}) {
        auto archive = scratch.path() / (m + ".7z");
        ASSERT_EQ(run_in(tree, "bsdtar --format 7zip --options 7zip:compression=" + m + " -cf '" + archive.string() + "' ."), 0) << m;
        auto bytes = slurp(archive);
        auto a = sevenzip::archive::from(compress_test::bytes(bytes));
        ASSERT_TRUE(a) << m << ": " << a.error().message();
        check_against_tree(*a, tree, "bsdtar " + m);
    }
}

// libarchive reads 7-Zip's BCJ2 archive as we do (its decoder of BCJ2 is
// the other one we have)
TEST(SevenZip_Tests, Bcj2AsLibarchiveReadsIt) {
    SKIP_WITHOUT_7ZZ();
    if (!have_bsdtar()) {
        GTEST_SKIP() << "bsdtar not found";
    }
    scratch_dir scratch("sgcl-7z");
    auto tree = make_tree(scratch.path());
    auto archive = scratch.path() / "bcj2.7z";
    ASSERT_EQ(run_in(tree, seven_zip() + " a -snl -mf=BCJ2 -m0=LZMA '" + archive.string() + "' ."), 0);
    fs::create_directories(scratch.path() / "out");
    ASSERT_EQ(run_in(scratch.path() / "out", "bsdtar -xf '" + archive.string() + "'"), 0);
    auto a = sevenzip::archive::open(sgcl::string(archive.string()));
    ASSERT_TRUE(a);
    for (auto& e : a->entries()) {
        if (e.is_directory || e.is_symlink()) {
            continue;
        }
        auto d = a->read(e);
        ASSERT_TRUE(d);
        EXPECT_EQ(text_of(*d), slurp(scratch.path() / "out" / std::string(e.name.view()))) << e.name;
    }
}

// A password not given: the entries are there, marked encrypted, their
// data asks for it; an encrypted header asks for it when the archive is
// opened
TEST(SevenZip_Tests, APasswordNotGivenIsNamed) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7z");
    auto tree = make_tree(scratch.path());
    auto enc = scratch.path() / "enc.7z";
    ASSERT_EQ(run_in(tree, seven_zip() + " a -snl -pSecret '" + enc.string() + "' ."), 0);
    auto a = sevenzip::archive::open(sgcl::string(enc.string()));
    ASSERT_TRUE(a) << a.error().message();
    auto e = a->find("text.txt");
    ASSERT_TRUE(e);
    EXPECT_TRUE(e->encrypted);
    EXPECT_EQ(e->size, 250000u);
    auto d = a->read(*e);
    ASSERT_FALSE(d);
    EXPECT_EQ(d.error().code(), sgcl::compress::errc::password_required);
    EXPECT_NE(std::string(d.error().message().view()).find("password required"), std::string::npos) << d.error().message();
    auto dir = a->find("empty-dir");
    ASSERT_TRUE(dir);
    EXPECT_FALSE(dir->encrypted);
    auto hidden = scratch.path() / "hidden.7z";
    ASSERT_EQ(run_in(tree, seven_zip() + " a -snl -pSecret -mhe=on '" + hidden.string() + "' ."), 0);
    auto h = sevenzip::archive::open(sgcl::string(hidden.string()));
    ASSERT_FALSE(h);
    EXPECT_EQ(h.error().code(), sgcl::compress::errc::password_required);
    EXPECT_NE(std::string(h.error().message().view()).find("password required"), std::string::npos) << h.error().message();
}

// An update into a new archive of what changed (7-Zip's -u switches):
// the deleted file and directory are anti-items
TEST(SevenZip_Tests, AntiItems) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7z");
    auto t = scratch.path() / "t";
    put(t / "a.txt", "one");
    put(t / "b.txt", "two");
    put(t / "d/c.txt", "three");
    ASSERT_EQ(run_in(t, seven_zip() + " a ../base.7z ."), 0);
    fs::remove(t / "b.txt");
    fs::remove_all(t / "d");
    put(t / "e.txt", "four");
    ASSERT_EQ(run_in(t, seven_zip() + " u ../base.7z -u- '-up0q3r2x2y2z0w2!../diff.7z' ."), 0);
    auto a = sevenzip::archive::open(sgcl::string((scratch.path() / "diff.7z").string()));
    ASSERT_TRUE(a) << a.error().message();
    ASSERT_EQ(a->entries().size(), 4u);
    auto b = a->find("b.txt");
    auto d = a->find("d");
    auto c = a->find("d/c.txt");
    auto e = a->find("e.txt");
    ASSERT_TRUE(b && d && c && e);
    EXPECT_TRUE(b->is_anti);
    EXPECT_FALSE(b->is_directory);
    EXPECT_TRUE(d->is_anti);
    EXPECT_TRUE(d->is_directory);
    EXPECT_TRUE(c->is_anti);
    EXPECT_FALSE(e->is_anti);
    EXPECT_EQ(text_of(value_of(a->read(*e))), "four");
}

// The walk: one reader at a time; one read only in part leaves the next
// entry right; a reader of an entry passed by is closed
TEST(SevenZip_Tests, TheWalkGoesOnInOrder) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7z");
    auto tree = make_tree(scratch.path());
    auto archive = scratch.path() / "w.7z";
    ASSERT_EQ(run_in(tree, seven_zip() + " a -snl -m0=LZMA2 '" + archive.string() + "' ."), 0);
    auto a = sevenzip::archive::open(sgcl::string(archive.string()));
    ASSERT_TRUE(a);
    std::optional<sgcl::io::reader> old;
    size_t i = 0;
    for (auto [e, r] : a->walk()) {
        if (old) {
            std::byte b[1];
            auto n = old->read(b);
            ASSERT_FALSE(n);
            EXPECT_EQ(n.error().code(), sgcl::error_code(sgcl::io::errc::closed));
        }
        if (i++ % 2 == 0 && e.size > 10) {
            std::byte part[7];
            ASSERT_TRUE(r.read(part));   // the rest dropped when the walk goes on
        } else {
            auto all = r.read_all();
            ASSERT_TRUE(all);
            EXPECT_EQ(text_of(*all), text_of(value_of(a->read(e)))) << e.name;
        }
        old = r;
    }
}

TEST(SevenZip_Tests, TheAsyncForms) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7z");
    auto tree = make_tree(scratch.path());
    auto archive = scratch.path() / "async.7z";
    ASSERT_EQ(run_in(tree, seven_zip() + " a -snl -mf=BCJ2 -m0=LZMA '" + archive.string() + "' ."), 0);
    auto expect = slurp(tree / "text.txt");
    auto task = sgcl::async::spawn([](std::string path, std::string expect) -> sgcl::async::task<std::string> {
        auto a = co_await sevenzip::archive::async_open(sgcl::string(path));
        if (!a) {
            co_return "open: " + std::string(a.error().message().view());
        }
        auto e = a->find("text.txt");
        auto d = co_await a->async_read(*e);
        if (!d || text_of(*d) != expect) {
            co_return "async_read";
        }
        auto r = a->reader(*e);
        auto all = co_await r->async_read_all();
        if (!all || text_of(*all) != expect) {
            co_return "reader";
        }
        size_t n = 0, bytes = 0;
        auto g = a->async_walk();
        while (auto v = co_await g.next()) {
            auto [entry, rd] = *v;
            auto data = co_await rd.async_read_all();
            if (!data) {
                co_return "walk " + std::string(entry.name.view());
            }
            bytes += data->size();
            ++n;
        }
        (void)a->close();
        co_return n == a->entries().size() && bytes > 0 ? std::string("ok") : std::string("walk count");
    }(archive.string(), expect));
    EXPECT_EQ(task.wait(), "ok");
    sgcl::async::scheduler::stop();
}

// max_entries against the header's count before the table is made;
// max_memory against a folder's decoders; max_size before a read
TEST(SevenZip_Tests, TheLimits) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7z");
    auto tree = make_tree(scratch.path());
    auto archive = scratch.path() / "l.7z";
    ASSERT_EQ(run_in(tree, seven_zip() + " a -snl -m0=LZMA:d=64m -mhc=off '" + archive.string() + "' ."), 0);
    auto path = sgcl::string(archive.string());
    auto few = sevenzip::archive::open(path, sgcl::compress::limits{.max_entries = 3});
    ASSERT_FALSE(few);
    EXPECT_EQ(few.error().code(), sgcl::compress::errc::too_large);
    auto a = sevenzip::archive::open(path);
    ASSERT_TRUE(a);
    auto e = a->find("text.txt");
    auto tight = a->read(*e, sgcl::compress::limits{.max_memory = 100000});
    ASSERT_FALSE(tight);
    EXPECT_EQ(tight.error().code(), sgcl::compress::errc::too_large);
    auto small = a->read(*e, sgcl::compress::limits{.max_size = 1000});
    ASSERT_FALSE(small);
    EXPECT_EQ(small.error().code(), sgcl::compress::errc::too_large);
    EXPECT_TRUE(a->read(*e));   // the window no larger than the folder: 64 MiB is not taken
}

// Cut short anywhere, a byte flipped anywhere: an error (the header's and
// the data's CRCs), never a crash; the walk agreeing with the reads
TEST(SevenZip_Tests, DamageIsFound) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7z");
    auto t = scratch.path() / "small";
    put(t / "a.txt", repo_text(3000));
    put(t / "b/c.txt", "short");
    for (std::string sw : {"-m0=LZMA2", "-m0=PPMd -mhc=off", "-mf=BCJ2 -m0=LZMA", "-m0=BZip2 -ms=off"}) {
        auto archive = scratch.path() / "d.7z";
        fs::remove(archive);
        ASSERT_EQ(run_in(t, seven_zip() + " a " + sw + " '" + archive.string() + "' ."), 0);
        auto good = slurp(archive);
        // every entry's name and data, read and walked; any_error when a step failed
        auto read_all = [](const std::string& bytes, bool& any_error) {
            any_error = false;
            std::string seen;
            auto a = sevenzip::archive::from(compress_test::bytes(bytes));
            if (!a) {
                any_error = true;
                return seen;
            }
            for (auto& e : a->entries()) {
                auto d = a->read(e);
                any_error |= !d;
                seen += std::string(e.name.view()) + "=" + (d ? text_of(*d) : std::string("?")) + ";";
            }
            for (auto [e, r] : a->walk()) {
                auto all = r.read_all();
                any_error |= !all;
            }
            return seen;
        };
        bool none;
        auto original = read_all(good, none);
        ASSERT_FALSE(none) << sw;
        for (size_t n = 0; n < good.size(); ++n) {
            bool err;
            read_all(good.substr(0, n), err);
            EXPECT_TRUE(err) << sw << " cut at " << n;
        }
        // a flip is found, or it changed nothing that is read (byte 7, the
        // minor version, which no CRC covers; a bzip2 stream's level digit
        // or its padding bits): never other bytes without an error
        std::mt19937 rng(1);
        for (int i = 0; i < 600; ++i) {
            auto bad = good;
            size_t at = rng() % bad.size();
            bad[at] = char(bad[at] ^ (1 << (rng() % 8)));
            bool err;
            auto seen = read_all(bad, err);
            EXPECT_TRUE(err || seen == original) << sw << " flip at " << at;
            if (sw.find("BZip2") == std::string::npos && at != 7) {
                EXPECT_TRUE(err) << sw << " flip at " << at;
            }
        }
    }
    // a name with an unpaired surrogate is corrupt (a plain header made by hand)
    // kHeader, kFilesInfo, one file; kEmptyStream (1 byte: it has none);
    // kName (5 bytes: not external, U+D800 alone, the terminator); the ends
    std::string h("\x01\x05\x01" "\x0E\x01\x80" "\x11\x05\x00\x00\xD8\x00\x00" "\x00\x00", 15);
    std::string arc = std::string("7z\xBC\xAF\x27\x1C\x00\x04", 8);
    std::string start(20, '\0');
    start[8] = char(h.size());
    uint32_t hcrc = sgcl::hash::crc32::of(compress_test::bytes(h));
    for (int i = 0; i < 4; ++i) start[16 + i] = char(hcrc >> (8 * i));
    uint32_t scrc = sgcl::hash::crc32::of(compress_test::bytes(start));
    for (int i = 0; i < 4; ++i) arc += char(scrc >> (8 * i));
    arc += start + h;
    auto bad = sevenzip::archive::from(compress_test::bytes(arc));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), sgcl::compress::errc::corrupt) << bad.error().message();
    EXPECT_NE(std::string(bad.error().message().view()).find("surrogate"), std::string::npos) << bad.error().message();
}

namespace {
    // What a command prints, run in a directory
    std::string capture(const fs::path& dir, const std::string& command) {
        std::string c = "cd '" + dir.string() + "' && " + command + " 2>&1";
        std::string out;
        if (FILE* f = ::popen(c.c_str(), "r")) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
                out.append(buf, n);
            }
            ::pclose(f);
        }
        return out;
    }

    // The tree written by our writer: in the order of the paths, links as
    // links, modes and times as the files have them
    sgcl::optional<sgcl::compress::error> write_tree(const fs::path& tree, const fs::path& archive, const sevenzip::options& o) {
        sevenzip::writer w(sgcl::string(archive.string()), o);
        std::vector<fs::path> all;
        for (auto& e : fs::recursive_directory_iterator(tree)) {
            all.push_back(e.path());
        }
        std::sort(all.begin(), all.end());
        for (auto& p : all) {
            std::string name = p.lexically_relative(tree).generic_string();
            auto st = fs::symlink_status(p);
            sevenzip::entry_info info;
            info.mode = sgcl::io::permissions(unsigned(st.permissions()) & 07777);
            if (!fs::is_symlink(st)) {
                auto sys = std::chrono::file_clock::to_sys(fs::last_write_time(p));
                info.modified = sgcl::time::datetime::from_unix_nano(std::chrono::duration_cast<std::chrono::nanoseconds>(sys.time_since_epoch()).count());
            }
            if (fs::is_symlink(st)) {
                info.symlink = true;
                w.add(sgcl::string(name), sgcl::string(fs::read_symlink(p).string()), info);
            } else if (fs::is_directory(st)) {
                w.add_directory(sgcl::string(name), info);
            } else {
                auto data = slurp(p);
                sgcl::io::writer e = w.create(sgcl::string(name), info);
                for (size_t i = 0; i < data.size(); i += 70001) {
                    e.write(compress_test::bytes(data.substr(i, 70001)));
                }
            }
        }
        auto r = w.close();
        if (!r) {
            return r.error();
        }
        return sgcl::nullopt;
    }

    // Two trees alike: names, kinds, link targets, contents, modes, times to the second
    void expect_same_tree(const fs::path& a, const fs::path& b, const std::string& what) {
        std::set<std::string> na, nb;
        for (auto& e : fs::recursive_directory_iterator(a)) na.insert(e.path().lexically_relative(a).generic_string());
        for (auto& e : fs::recursive_directory_iterator(b)) nb.insert(e.path().lexically_relative(b).generic_string());
        ASSERT_EQ(na, nb) << what;
        for (auto& n : na) {
            auto pa = a / n, pb = b / n;
            auto sa = fs::symlink_status(pa), sb = fs::symlink_status(pb);
            ASSERT_EQ(sa.type(), sb.type()) << what << " " << n;
            if (fs::is_symlink(sa)) {
                EXPECT_EQ(fs::read_symlink(pa), fs::read_symlink(pb)) << what << " " << n;
                continue;
            }
            EXPECT_EQ(sa.permissions(), sb.permissions()) << what << " " << n;
            if (fs::is_regular_file(sa)) {
                ASSERT_EQ(slurp(pa), slurp(pb)) << what << " " << n;
                auto ta = std::chrono::file_clock::to_sys(fs::last_write_time(pa));
                auto tb = std::chrono::file_clock::to_sys(fs::last_write_time(pb));
                EXPECT_LE(std::abs(std::chrono::duration_cast<std::chrono::seconds>(ta - tb).count()), 1) << what << " " << n;
            }
        }
    }
}

// Our archives of every method, solid and not, levels and filters, read
// by 7-Zip (7zz t, and 7zz x compared with the tree), by libarchive where
// it reads them, and by our own reader
TEST(SevenZip_Tests, OurArchivesReadByTheOracles) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7zw");
    auto tree = make_tree(scratch.path());
    struct Case {
        std::string name;
        sevenzip::options o;
    };
    std::vector<Case> cases = {   // lint-handles: ok every options keeps its slice optional empty
        {"lzma2", {}},
        {"lzma2-nonsolid", {.solid = false}},
        {"lzma2-0", {.level = 0}},
        {"lzma2-9", {.level = 9}},
        {"lzma", {.method = sevenzip::method::lzma, .level = 5}},
        {"ppmd", {.method = sevenzip::method::ppmd}},
        {"ppmd-1", {.method = sevenzip::method::ppmd, .level = 1}},
        {"ppmd-8", {.method = sevenzip::method::ppmd, .level = 8}},
        {"deflate", {.method = sevenzip::method::deflate}},
        {"copy", {.method = sevenzip::method::copy}},
        {"blocks", {.solid_block = 50000}},
        {"nofilters", {.auto_filters = false}},
    };
    for (auto& c : cases) {
        auto archive = scratch.path() / (c.name + ".7z");
        auto e = write_tree(tree, archive, c.o);
        ASSERT_FALSE(e) << c.name << ": " << e->message();
        auto test = capture(scratch.path(), seven_zip() + " t '" + archive.string() + "'");
        EXPECT_NE(test.find("Everything is Ok"), std::string::npos) << c.name << ": " << test;
        auto out = scratch.path() / ("x-" + c.name);
        fs::create_directories(out);
        ASSERT_EQ(run_in(out, seven_zip() + " x -snl '" + archive.string() + "'"), 0) << c.name;
        expect_same_tree(tree, out, "7zz x " + c.name);
        if (have_bsdtar()) {
            auto bx = scratch.path() / ("b-" + c.name);
            fs::create_directories(bx);
            EXPECT_EQ(run_in(bx, "bsdtar -xf '" + archive.string() + "'"), 0) << "bsdtar " << c.name;
            for (auto n : {"text.txt", "dir/prog.bin", "dir/random.bin"}) {
                EXPECT_EQ(slurp(bx / n), slurp(tree / n)) << "bsdtar " << c.name << " " << n;
            }
        }
        auto a = sevenzip::archive::open(sgcl::string(archive.string()));
        ASSERT_TRUE(a) << c.name << ": " << a.error().message();
        check_against_tree(*a, tree, "ours " + c.name);
    }
}

// PPMd of every order 2..32 and memories from 1 MiB, written as a folder
// of our own (the writer takes the order and memory from the level): 7-Zip
// reads them, which closes what S3 left to the round trip
TEST(SevenZip_Tests, PpmdOrdersAndMemoriesRead7Zip) {
    SKIP_WITHOUT_7ZZ();
    namespace wr = sgcl::compress::detail::sevenzip_writing;
    scratch_dir scratch("sgcl-7zw");
    auto text = repo_text(200000);
    for (uint32_t order : {2u, 3u, 5u, 8u, 13u, 16u, 24u, 32u}) {
        for (uint32_t mem : {uint32_t(1) << 20, uint32_t(3) << 20, uint32_t(16) << 20}) {
            auto enc = std::make_unique<sgcl::compress::detail::Ppmd7Encoder>(order, mem);
            std::vector<uint8_t> packed;
            enc->encode(reinterpret_cast<const uint8_t*>(text.data()), text.size(), packed);
            enc->finish(packed);
            wr::FolderRecord f;
            wr::CoderRecord c;
            c.method = sgcl::compress::detail::sevenzip_format::Ppmd;
            c.props.push_back(uint8_t(order));
            for (int i = 0; i < 4; ++i) c.props.push_back(uint8_t(mem >> (8 * i)));
            f.coders.push_back(c);
            f.unpacked = text.size();
            f.packed = packed.size();
            f.sizes.push_back(text.size());
            f.crcs.push_back(sgcl::hash::crc32::of(compress_test::bytes(text)));
            wr::FileRecord r;
            r.name = u"text.txt";
            r.stream = true;
            r.size = text.size();
            r.crc = f.crcs[0];
            r.attributes = 0x8020 | (0100644u << 16);
            std::vector<uint8_t> out(32, 0);
            out.insert(out.end(), packed.begin(), packed.end());
            auto plain = wr::header({f}, {r});
            std::vector<uint8_t> hp, he;
            wr::packed_header(plain, packed.size(), hp, he);
            out.insert(out.end(), hp.begin(), hp.end());
            out.insert(out.end(), he.begin(), he.end());
            auto sig = wr::signature(packed.size() + hp.size(), he);
            std::copy(sig.begin(), sig.end(), out.begin());
            auto path = scratch.path() / ("o" + std::to_string(order) + "m" + std::to_string(mem) + ".7z");
            std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
            auto x = scratch.path() / ("x" + std::to_string(order) + "-" + std::to_string(mem));
            fs::create_directories(x);
            ASSERT_EQ(run_in(x, seven_zip() + " x '" + path.string() + "'"), 0) << order << " " << mem;
            EXPECT_EQ(slurp(x / "text.txt"), text) << order << " " << mem;
        }
    }
}

// More than 65 535 entries once (7-Zip's numbers take them all), several
// folders by a small block, and a writer's folder per entry when not solid
TEST(SevenZip_Tests, ManyEntriesAndFolders) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7zw");
    auto archive = scratch.path() / "many.7z";
    {
        sevenzip::writer w(sgcl::string(archive.string()), {.level = 1, .solid_block = uint64_t(1) << 20});
        for (int i = 0; i < 70000; ++i) {
            if (i % 1000 == 0) {
                w.add_directory(sgcl::string("d" + std::to_string(i / 1000)));
            }
            w.add(sgcl::string("d" + std::to_string(i / 1000) + "/f" + std::to_string(i)), sgcl::string(std::string(i % 50, char('a' + i % 26))));
        }
        ASSERT_TRUE(w.close());
    }
    auto test = capture(scratch.path(), seven_zip() + " t '" + archive.string() + "'");
    EXPECT_NE(test.find("Everything is Ok"), std::string::npos) << test.substr(0, 2000);
    EXPECT_NE(test.find("Files: 70000"), std::string::npos) << test.substr(test.size() > 300 ? test.size() - 300 : 0);
    auto a = sevenzip::archive::open(sgcl::string(archive.string()));
    ASSERT_TRUE(a);
    EXPECT_EQ(a->entries().size(), 70070u);
    auto e = a->find("d69/f69999");
    ASSERT_TRUE(e);
    EXPECT_EQ(text_of(value_of(a->read(*e))), std::string(69999 % 50, char('a' + 69999 % 26)));
    std::set<uint32_t> folders;
    for (auto& x : a->entries()) {
        if (x.folder != UINT32_MAX) folders.insert(x.folder);
    }
    EXPECT_GT(folders.size(), 1u);
}

// The automatic filters by the first bytes: ELF, Mach-O and PE of each
// processor, a WAV file; 7-Zip names the filter it reads, and decodes
TEST(SevenZip_Tests, TheAutomaticFilters) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7zw");
    auto body = program(60000);
    auto elf = [&](uint16_t machine, bool big = false) {
        std::string h(64, '\0');
        h[0] = 0x7F; h[1] = 'E'; h[2] = 'L'; h[3] = 'F'; h[4] = 2; h[5] = big ? 2 : 1;
        h[18] = char(big ? machine >> 8 : machine); h[19] = char(big ? machine : machine >> 8);
        return h + body;
    };
    auto macho = [&](uint32_t cpu) {
        std::string h("\xCF\xFA\xED\xFE", 4);
        for (int i = 0; i < 4; ++i) h += char(cpu >> (8 * i));
        h.resize(64, '\0');
        return h + body;
    };
    auto pe = [&](uint16_t machine) {
        std::string h(0x100, '\0');
        h[0] = 'M'; h[1] = 'Z'; h[0x3C] = char(0x80);
        h.replace(0x80, 4, std::string("PE\0\0", 4));
        h[0x84] = char(machine); h[0x85] = char(machine >> 8);
        return h + body;
    };
    std::string wav("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\x44\xAC\0\0\x10\xB1\x02\0\x04\0\x10\0", 36);
    wav += body;
    struct Case {
        std::string file;
        std::string data;
        std::string method;   // as 7-Zip names it
    };
    std::vector<Case> cases = {
        {"x64.elf", elf(0x3E), "BCJ"}, {"arm64.elf", elf(0xB7), "ARM64"}, {"riscv.elf", elf(0xF3), "RISCV"},
        {"arm.elf", elf(0x28), "ARM"}, {"ppc.elf", elf(0x14, true), "PPC"}, {"sparc.elf", elf(0x2B, true), "SPARC"},
        {"ia64.elf", elf(0x32), "IA64"}, {"arm64.macho", macho(0x0100000C), "ARM64"}, {"x64.macho", macho(0x01000007), "BCJ"},
        {"x86.exe", pe(0x14C), "BCJ"}, {"arm64.exe", pe(0xAA64), "ARM64"}, {"thumb.exe", pe(0x1C4), "ARMT"},
        {"sound.wav", wav, "Delta:4"}, {"plain.bin", body, ""},
    };
    for (auto& c : cases) {
        auto archive = scratch.path() / (c.file + ".7z");
        {
            sevenzip::writer w(sgcl::string(archive.string()));
            w.add(sgcl::string(c.file), compress_test::bytes(c.data));
            ASSERT_TRUE(w.close()) << c.file;
        }
        auto list = capture(scratch.path(), seven_zip() + " l -slt '" + archive.string() + "'");
        auto at = list.rfind("Method = ");
        ASSERT_NE(at, std::string::npos) << list;
        std::string method = list.substr(at + 9, list.find('\n', at) - at - 9);
        if (c.method.empty()) {
            EXPECT_EQ(method.find(' '), std::string::npos) << c.file << ": " << method;
        } else {
            EXPECT_EQ(method.rfind(c.method + " ", 0), 0u) << c.file << ": " << method;
        }
        EXPECT_NE(capture(scratch.path(), seven_zip() + " t '" + archive.string() + "'").find("Everything is Ok"), std::string::npos) << c.file;
        auto a = sevenzip::archive::open(sgcl::string(archive.string()));
        ASSERT_TRUE(a);
        EXPECT_EQ(text_of(value_of(a->read(a->entries()[0]))), c.data) << c.file;
    }
}

// Every error is the first: after it nothing is written, close() gives it;
// a write to an entry that ended fails by itself, kept by nothing; a path
// that cannot be made
TEST(SevenZip_Tests, AWriterKeepsItsFirstError) {
    scratch_dir scratch("sgcl-7zw");
    {
        sevenzip::writer w(sgcl::string((scratch.path() / "a.7z").string()));
        w.add("good.txt", "fine");
        w.add(sgcl::string(std::string("bad\xFF", 4)), "never");
        ASSERT_TRUE(w.last_error());
        EXPECT_EQ(w.last_error()->code(), sgcl::compress::errc::invalid_argument);
        w.add("after.txt", "refused");
        auto r = w.close();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), sgcl::compress::errc::invalid_argument);
        EXPECT_FALSE(w.close());   // the same error again
    }
    {
        sevenzip::writer w(sgcl::string((scratch.path() / "b.7z").string()));
        sgcl::io::writer first = w.create("one.txt");
        ASSERT_TRUE(first.write("1"));
        w.add("two.txt", "2");
        // the entry ended: the write fails by itself, the archive goes on
        auto late = first.write("late");
        ASSERT_FALSE(late);
        EXPECT_EQ(late.error().code(), sgcl::error_code(sgcl::io::errc::closed));
        EXPECT_FALSE(w.last_error());
        sgcl::io::writer third = w.create("three.txt");
        ASSERT_TRUE(third.write("3"));
        ASSERT_TRUE(third.close());
        auto closed_entry = third.write("late");   // after its own close
        ASSERT_FALSE(closed_entry);
        EXPECT_EQ(closed_entry.error().code(), sgcl::error_code(sgcl::io::errc::closed));
        auto t = sgcl::async::spawn([](sgcl::io::writer stale) -> sgcl::async::task<bool> {
            auto r = co_await stale.async_write(std::string("late"));
            co_return !r && r.error().is_closed();
        }(first));
        EXPECT_TRUE(t.wait());
        EXPECT_FALSE(w.last_error());
        EXPECT_FALSE(w.is_closed());
        ASSERT_TRUE(w.close());
        EXPECT_TRUE(w.is_closed());
        auto after = third.write("late");   // after the writer's close
        ASSERT_FALSE(after);
        EXPECT_EQ(after.error().code(), sgcl::error_code(sgcl::io::errc::closed));
        EXPECT_FALSE(w.last_error());
        ASSERT_TRUE(w.close());
        auto a = sevenzip::archive::open(sgcl::string((scratch.path() / "b.7z").string()));
        ASSERT_TRUE(a);
        EXPECT_EQ(a->entries().size(), 3u);
        sgcl::async::scheduler::stop();
    }
    {
        sevenzip::writer w(sgcl::string((scratch.path() / "no/such/dir/c.7z").string()));
        w.add("x.txt", "x");
        auto r = w.close();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), sgcl::compress::errc::io);
    }
    {
        sevenzip::writer w(sgcl::string((scratch.path() / "d.7z").string()));
        w.add_directory("dir");
        sgcl::io::writer data_for_dir = w.create("file");
        (void)data_for_dir.close();
        w.add("x", "x");
        ASSERT_TRUE(w.close());
        auto a = sevenzip::archive::open(sgcl::string((scratch.path() / "d.7z").string()));
        ASSERT_TRUE(a);
        ASSERT_EQ(a->entries().size(), 3u);
        EXPECT_TRUE(a->entries()[0].is_directory);
        EXPECT_FALSE(a->entries()[1].is_directory);   // an empty file
        EXPECT_EQ(a->entries()[1].size, 0u);
    }
}

// Into a buffer after what it holds (sought back for the signature
// header), the task's forms, and an archive of nothing (32 bytes)
TEST(SevenZip_Tests, IntoABufferAndInATask) {
    sgcl::io::buffer b;
    ASSERT_TRUE(b.write("prefix"));
    auto text = repo_text(300000);
    auto task = sgcl::async::spawn([](sgcl::io::buffer b, std::string text) -> sgcl::async::task<std::string> {
        sevenzip::writer w(b, {.method = sevenzip::method::ppmd, .level = 4});
        sgcl::io::writer e = w.create("text.txt");
        for (size_t i = 0; i < text.size(); i += 100000) {
            if (!co_await e.async_write(compress_test::bytes(text.substr(i, 100000)))) {
                co_return "write";
            }
        }
        w.add_directory("d");
        auto r = co_await w.async_close();
        co_return r ? std::string("ok") : std::string(r.error().message().view());
    }(b, text));
    ASSERT_EQ(task.wait(), "ok");
    sgcl::async::scheduler::stop();
    auto all = b.data();
    ASSERT_EQ(std::string(reinterpret_cast<const char*>(all.data()), 6), "prefix");
    auto a = sevenzip::archive::from(all.subslice(6));
    ASSERT_TRUE(a) << a.error().message();
    EXPECT_EQ(text_of(value_of(a->read("text.txt"))), text);
    sgcl::io::buffer empty;
    {
        sevenzip::writer w(empty);
        ASSERT_TRUE(w.close());
    }
    EXPECT_EQ(empty.size(), 32u);
    auto none = sevenzip::archive::from(empty.data());
    ASSERT_TRUE(none) << none.error().message();
    EXPECT_TRUE(none->entries().empty());
    if (!seven_zip().empty()) {
        scratch_dir scratch("sgcl-7zw");
        std::ofstream(scratch.path() / "empty.7z", std::ios::binary).write(reinterpret_cast<const char*>(empty.data().data()), 32);
        EXPECT_EQ(run_in(scratch.path(), seven_zip() + " t empty.7z"), 0);
    }
}

// ---- 7zAES -----------------------------------------------------------------

namespace {
    namespace aes7 = sgcl::compress::detail::sevenzip_aes;

    std::vector<uint8_t> hex(const std::string& h) {
        std::vector<uint8_t> out;
        for (size_t i = 0; i + 1 < h.size(); i += 2) {
            out.push_back(uint8_t(std::stoi(h.substr(i, 2), nullptr, 16)));
        }
        return out;
    }

    sgcl::crypto::secret<32> key_of(const std::vector<uint8_t>& k) {
        auto s = sgcl::crypto::detail::SecretAccess::make<32>();
        std::memcpy(sgcl::crypto::detail::SecretAccess::data(s), k.data(), 32);
        return s;
    }

    std::vector<uint8_t> key_bytes(const sgcl::crypto::secret<32>& k) {
        auto b = k.bytes();
        return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(b.data()), reinterpret_cast<const uint8_t*>(b.data()) + 32);
    }

    // SHA-256 of one message, from another implementation where there is one
    std::vector<uint8_t> sha256_apart(const std::string& m) {
        std::vector<uint8_t> d(32);
#if defined(__APPLE__)
        CC_SHA256(m.data(), CC_LONG(m.size()), d.data());
#else
        auto v = sgcl::crypto::sha256::of(compress_test::bytes(m));
        std::memcpy(d.data(), v.data(), 32);
#endif
        return d;
    }

    // 7-Zip's key, computed as its description reads: every round's bytes
    // (the salt, the password in UTF-16LE, the round's number of 8 bytes)
    // one after another in one message, and that message hashed once
    std::vector<uint8_t> key_apart(const std::u16string& password, const std::string& salt, uint32_t k) {
        std::string pw;
        for (char16_t c : password) {
            pw += char(c & 0xFF);
            pw += char(c >> 8);
        }
        if (k == 0x3F) {
            std::string key = salt + pw;
            key.resize(32, '\0');
            return std::vector<uint8_t>(key.begin(), key.end());
        }
        std::string m;
        for (uint64_t i = 0; i < (uint64_t(1) << k); ++i) {
            m += salt + pw;
            for (int b = 0; b < 8; ++b) {
                m += char(i >> (8 * b));
            }
        }
        return sha256_apart(m);
    }

    // What 7-Zip prints and its exit code
    struct Run {
        int code;
        std::string out;
    };

    Run run_7zz(const fs::path& dir, const std::string& args) {
        std::string c = "cd '" + dir.string() + "' && " + seven_zip() + " " + args + " < /dev/null 2>&1";
        Run r{0, {}};
        if (FILE* f = ::popen(c.c_str(), "r")) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
                r.out.append(buf, n);
            }
            r.code = ::pclose(f);
        }
        return r;
    }

    sevenzip::options with_password(const std::string& p) {
        sevenzip::options o;
        o.password = sgcl::string(p);
        return o;
    }

    // A program 7-Zip's filter is chosen for (an ELF of ARM64), put in the tree
    void put_program(const fs::path& tree) {
        std::string h(64, '\0');
        h[0] = 0x7F; h[1] = 'E'; h[2] = 'L'; h[3] = 'F'; h[4] = 2; h[5] = 1; h[18] = char(0xB7);
        put(tree / "dir/arm64.elf", h + program(80000));
    }
}

// AES-256-CBC on NIST SP 800-38A F.2.5 (encryption) and F.2.6 (decryption),
// whole and in pieces (the chain carried from call to call)
TEST(SevenZipAes_Tests, CbcOnTheNistVectors) {
    auto key = key_of(hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4"));
    auto iv = hex("000102030405060708090a0b0c0d0e0f");
    auto plain = hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
                     "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    auto cipher = hex("f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d"
                      "39f23369a9d9bacfa530e26304231461b2eb05e2c39be9fcda6c19078c6a9d1b");
    auto b = plain;
    aes7::Cbc(key, iv.data()).encrypt(b.data(), b.size());
    EXPECT_EQ(b, cipher);
    aes7::Cbc(key, iv.data()).decrypt(b.data(), b.size());
    EXPECT_EQ(b, plain);
    for (size_t cut : {16u, 32u, 48u}) {
        auto e = plain;
        aes7::Cbc enc(key, iv.data());
        enc.encrypt(e.data(), cut);
        enc.encrypt(e.data() + cut, e.size() - cut);
        EXPECT_EQ(e, cipher) << cut;
        aes7::Cbc dec(key, iv.data());
        dec.decrypt(e.data(), cut);
        dec.decrypt(e.data() + cut, e.size() - cut);
        EXPECT_EQ(e, plain) << cut;
    }
}

// The key of a password: against one computed apart (every round in one
// message, hashed by the system's SHA-256), of an ASCII password and one
// past the BMP (a surrogate pair in UTF-16), with a salt and without, at
// several k, and k = 63 (no hashing); the properties parsed and written
TEST(SevenZipAes_Tests, TheKeyComputedApart) {
    struct Case {
        std::string utf8;
        std::u16string utf16;
    };
    std::vector<Case> passwords = {
        {"Secret", u"Secret"},
        {"za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 \xF0\x9F\x98\x80", u"zażółć \U0001F600"},
        {"", u""},
    };
    std::string salts[] = {"", "0123456789abcdef", "salt"};
    for (auto& pw : passwords) {
        aes7::Password password(pw.utf8);
        for (auto& salt : salts) {
            for (uint32_t k : {0u, 1u, 6u, 12u, 0x3Fu}) {
                aes7::Props p;
                p.k = k;
                p.salt_size = salt.size();
                std::memcpy(p.salt, salt.data(), salt.size());
                EXPECT_EQ(key_bytes(aes7::derive(password, p)), key_apart(pw.utf16, salt, k)) << pw.utf8 << " salt " << salt.size() << " k " << k;
            }
        }
    }
    // 7-Zip's k at a salt of 16 bytes, once
    aes7::Password password("Secret");
    aes7::Props p;
    p.k = 19;
    p.salt_size = 16;
    std::memcpy(p.salt, "0123456789abcdef", 16);
    EXPECT_EQ(key_bytes(aes7::derive(password, p)), key_apart(u"Secret", "0123456789abcdef", 19));
    // the properties as the writer makes them, read back
    uint8_t salt[16], iv[16];
    for (int i = 0; i < 16; ++i) {
        salt[i] = uint8_t(i * 7);
        iv[i] = uint8_t(200 - i);
    }
    auto bytes = aes7::props(19, salt, iv);
    ASSERT_EQ(bytes.size(), 34u);
    aes7::Props back;
    ASSERT_TRUE(aes7::parse(bytes, back));
    EXPECT_EQ(back.k, 19u);
    EXPECT_EQ(back.salt_size, 16u);
    EXPECT_EQ(std::memcmp(back.salt, salt, 16), 0);
    EXPECT_EQ(std::memcmp(back.iv, iv, 16), 0);
    // 7-Zip's own: no salt, an IV of 8 bytes, padded with zeros
    std::vector<uint8_t> seven = {0x40 | 19, 0x07, 1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_TRUE(aes7::parse(seven, back));
    EXPECT_EQ(back.salt_size, 0u);
    EXPECT_EQ(back.iv[7], 8);
    EXPECT_EQ(back.iv[8], 0);
    EXPECT_FALSE(aes7::parse({0xC0 | 19, 0x07, 1, 2}, back));   // shorter than its sizes
    EXPECT_FALSE(aes7::parse({}, back));
}

// 7-Zip's archives with a password (the header plain and encrypted; LZMA2,
// PPMd, Copy, a filter, BCJ2's four streams each encrypted): read with the
// password, and without it or with a wrong one one error each
TEST(SevenZipAes_Tests, SevenZipArchivesWithAPassword) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7zaes");
    auto tree = make_tree(scratch.path());
    put_program(tree);
    std::vector<std::pair<std::string, std::string>> cases = {
        {"lzma2", "-m0=LZMA2"}, {"ppmd", "-m0=PPMd"}, {"copy", "-m0=Copy"}, {"arm64", "-mf=ARM64 -m0=LZMA2"},
        {"bcj2", "-mf=BCJ2 -m0=LZMA"}, {"nonsolid", "-ms=off"},
    };
    for (auto& [name, switches] : cases) {
        for (bool hidden : {false, true}) {
            std::string what = name + (hidden ? " -mhe=on" : "");
            auto archive = scratch.path() / (name + (hidden ? "-he" : "") + ".7z");
            ASSERT_EQ(run_in(tree, seven_zip() + " a -snl -pSecret " + switches + (hidden ? " -mhe=on" : "") + " '" + archive.string() + "' ."), 0) << what;
            auto a = sevenzip::archive::open(sgcl::string(archive.string()), with_password("Secret"));
            ASSERT_TRUE(a) << what << ": " << a.error().message();
            check_against_tree(*a, tree, what);
            auto bytes = slurp(archive);
            auto m = sevenzip::archive::from(compress_test::bytes(bytes), with_password("Secret"));
            ASSERT_TRUE(m) << what;
            EXPECT_EQ(text_of(value_of(m->read("text.txt"))), slurp(tree / "text.txt")) << what;
            // no password, a wrong one
            auto none = sevenzip::archive::open(sgcl::string(archive.string()));
            auto wrong = sevenzip::archive::open(sgcl::string(archive.string()), with_password("Wrong"));
            if (hidden) {
                ASSERT_FALSE(none) << what;
                EXPECT_EQ(none.error().code(), sgcl::compress::errc::password_required) << what;
                ASSERT_FALSE(wrong) << what;
                EXPECT_EQ(wrong.error().code(), sgcl::compress::errc::wrong_password) << what << ": " << wrong.error().message();
                EXPECT_NE(std::string(wrong.error().message().view()).find("7z: wrong password"), std::string::npos) << wrong.error().message();
                continue;
            }
            ASSERT_TRUE(none) << what;
            ASSERT_TRUE(wrong) << what;
            for (auto n : {"text.txt", "dir/random.bin", "dir/arm64.elf"}) {
                auto d = none->read(n);
                ASSERT_FALSE(d) << what << " " << n;
                EXPECT_EQ(d.error().code(), sgcl::compress::errc::password_required) << what << " " << n;
                auto w = wrong->read(n);
                ASSERT_FALSE(w) << what << " " << n;
                EXPECT_EQ(w.error().code(), sgcl::compress::errc::wrong_password) << what << " " << n << ": " << w.error().message();
                EXPECT_NE(std::string(w.error().message().view()).find("7z: wrong password"), std::string::npos) << w.error().message();
            }
            size_t failed = 0;
            for (auto [e, r] : wrong->walk()) {
                if (e.size && !r.read_all()) {
                    ++failed;
                }
            }
            EXPECT_GE(failed, 3u) << what;
        }
    }
}

// Our archives with a password, the header plain and encrypted, of every
// method (and a filter): 7-Zip tests and extracts them with the password,
// names the coders, refuses a wrong one; without the password it lists the
// names of a plain header and cannot open an encrypted one; our reader
// reads them back
TEST(SevenZipAes_Tests, OurArchivesWithAPassword) {
    SKIP_WITHOUT_7ZZ();
    scratch_dir scratch("sgcl-7zaesw");
    auto tree = make_tree(scratch.path());
    put_program(tree);
    struct Case {
        std::string name;
        sevenzip::method method;
    };
    std::vector<Case> cases = {
        {"lzma2", sevenzip::method::lzma2}, {"ppmd", sevenzip::method::ppmd}, {"copy", sevenzip::method::copy},
        {"lzma", sevenzip::method::lzma}, {"deflate", sevenzip::method::deflate},
    };
    for (auto& c : cases) {
        for (bool hidden : {true, false}) {
            std::string what = c.name + (hidden ? " encrypted header" : " plain header");
            auto archive = scratch.path() / (c.name + (hidden ? "-he" : "") + ".7z");
            auto o = with_password("Secret");
            o.method = c.method;
            o.encrypt_header = hidden;
            auto err = write_tree(tree, archive, o);
            ASSERT_FALSE(err) << what << ": " << err->message();
            auto t = run_7zz(scratch.path(), "t -pSecret '" + archive.string() + "'");
            EXPECT_EQ(t.code, 0) << what << ": " << t.out;
            EXPECT_NE(t.out.find("Everything is Ok"), std::string::npos) << what << ": " << t.out;
            auto l = run_7zz(scratch.path(), "l -slt -pSecret '" + archive.string() + "'");
            EXPECT_NE(l.out.find("7zAES:19"), std::string::npos) << what << ": " << l.out.substr(0, 3000);
            if (c.method != sevenzip::method::copy) {
                EXPECT_NE(l.out.find("ARM64"), std::string::npos) << what;
            }
            auto out = scratch.path() / ("x-" + c.name + (hidden ? "-he" : ""));
            fs::create_directories(out);
            ASSERT_EQ(run_in(out, seven_zip() + " x -snl -pSecret '" + archive.string() + "'"), 0) << what;
            expect_same_tree(tree, out, "7zz x " + what);
            auto wrong = run_7zz(scratch.path(), "t -pWrong '" + archive.string() + "'");
            EXPECT_NE(wrong.code, 0) << what << ": " << wrong.out;
            EXPECT_NE(wrong.out.find("Wrong password"), std::string::npos) << what << ": " << wrong.out;
            auto names = run_7zz(scratch.path(), "l '" + archive.string() + "'");
            if (hidden) {
                EXPECT_EQ(names.out.find("text.txt"), std::string::npos) << what << ": " << names.out;
            } else {
                EXPECT_NE(names.out.find("text.txt"), std::string::npos) << what << ": " << names.out;
            }
            auto a = sevenzip::archive::open(sgcl::string(archive.string()), with_password("Secret"));
            ASSERT_TRUE(a) << what << ": " << a.error().message();
            check_against_tree(*a, tree, "ours " + what);
            auto bad = sevenzip::archive::open(sgcl::string(archive.string()), with_password("secret"));
            if (hidden) {
                ASSERT_FALSE(bad) << what;
                EXPECT_EQ(bad.error().code(), sgcl::compress::errc::wrong_password) << what;
            } else {
                ASSERT_TRUE(bad) << what;
                auto d = bad->read("text.txt");
                ASSERT_FALSE(d) << what;
                EXPECT_EQ(d.error().code(), sgcl::compress::errc::wrong_password) << what;
            }
        }
    }
}

// Round trips in memory: sizes around the block (0..33 bytes, the padding
// of the last one), solid and not, only directories with the header
// encrypted, a writer in a task; the key not kept in the options
TEST(SevenZipAes_Tests, RoundTrips) {
    sgcl::io::buffer b;
    {
        auto o = with_password("p\xC3\xA4ss");
        o.solid = false;
        o.level = 1;
        sevenzip::writer w(b, o);
        for (size_t n = 0; n <= 33; ++n) {
            w.add(sgcl::string("f" + std::to_string(n)), compress_test::bytes(std::string(n, char('a' + n % 26))));
        }
        w.add("big", compress_test::bytes(repo_text(200000)));
        ASSERT_TRUE(w.close());
    }
    auto a = sevenzip::archive::from(b.data(), with_password("p\xC3\xA4ss"));
    ASSERT_TRUE(a) << a.error().message();
    for (size_t n = 0; n <= 33; ++n) {
        auto d = a->read(sgcl::string("f" + std::to_string(n)));
        ASSERT_TRUE(d) << n << ": " << d.error().message();
        EXPECT_EQ(text_of(*d), std::string(n, char('a' + n % 26))) << n;
        auto e = a->find(sgcl::string("f" + std::to_string(n)));
        EXPECT_EQ(e->encrypted, n != 0) << n;
    }
    EXPECT_EQ(text_of(value_of(a->read("big"))), repo_text(200000));
    // readers from several threads at once: the keys shared, made once
    {
        auto opened = sevenzip::archive::from(b.data(), with_password("p\xC3\xA4ss"));
        ASSERT_TRUE(opened);
        std::atomic<int> good{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&, t] {
                for (size_t n = size_t(t); n <= 33; n += 4) {
                    auto d = opened->read(sgcl::string("f" + std::to_string(n)));
                    good += d && text_of(*d) == std::string(n, char('a' + n % 26));
                }
            });
        }
        for (auto& t : threads) {
            t.join();
        }
        EXPECT_EQ(good.load(), 34);
    }
    // only directories: no folder, the header encrypted
    sgcl::io::buffer dirs;
    {
        sevenzip::writer w(dirs, with_password("x"));
        w.add_directory("d1");
        w.add_directory("d1/d2");
        ASSERT_TRUE(w.close());
    }
    EXPECT_FALSE(sevenzip::archive::from(dirs.data()));
    auto d = sevenzip::archive::from(dirs.data(), with_password("x"));
    ASSERT_TRUE(d) << d.error().message();
    EXPECT_EQ(d->entries().size(), 2u);
    // a task's writer, and async_open with the password
    scratch_dir scratch("sgcl-7zaesrt");
    auto path = (scratch.path() / "t.7z").string();
    auto text = repo_text(150000);
    auto task = sgcl::async::spawn([](std::string path, std::string text) -> sgcl::async::task<std::string> {
        {
            sevenzip::writer w(sgcl::string(path), with_password("Secret"));
            sgcl::io::writer e = w.create("text.txt");
            if (!co_await e.async_write(compress_test::bytes(text))) {
                co_return "write";
            }
            auto r = co_await w.async_close();
            if (!r) {
                co_return std::string(r.error().message().view());
            }
        }
        auto a = co_await sevenzip::archive::async_open(sgcl::string(path), with_password("Secret"));
        if (!a) {
            co_return std::string(a.error().message().view());
        }
        auto d = co_await a->async_read(*a->find("text.txt"));
        if (!d || text_of(*d) != text) {
            co_return "read";
        }
        auto n = co_await sevenzip::archive::async_open(sgcl::string(path));
        co_return n ? std::string("opened without a password") : std::string(n.error().message().view());
    }(path, text));
    auto said = task.wait();
    EXPECT_NE(said.find("7z: password required"), std::string::npos) << said;
    sgcl::async::scheduler::stop();
    if (!seven_zip().empty()) {
        std::ofstream(scratch.path() / "rt.7z", std::ios::binary).write(reinterpret_cast<const char*>(b.data().data()), std::streamsize(b.size()));
        auto t = run_7zz(scratch.path(), "t -pp\xC3\xA4ss rt.7z");
        EXPECT_NE(t.out.find("Everything is Ok"), std::string::npos) << t.out;
        EXPECT_NE(run_7zz(scratch.path(), "t -pSecret t.7z").out.find("Everything is Ok"), std::string::npos);
    }
}

// A folder made by hand: 7zAES over Copy at k = 63 (the key the salt and
// the password themselves, which 7-Zip reads too), and at k = 25, past the
// limit, refused before any round is run
TEST(SevenZipAes_Tests, NoHashingAndPastTheLimit) {
    namespace wr = sgcl::compress::detail::sevenzip_writing;
    auto make = [](uint32_t k, const std::string& data, const std::string& password) {
        uint8_t salt[16], iv[16];
        for (int i = 0; i < 16; ++i) {
            salt[i] = uint8_t(i + 1);
            iv[i] = uint8_t(0xA0 + i);
        }
        aes7::Props p;
        p.k = k;
        p.salt_size = 16;
        std::memcpy(p.salt, salt, 16);
        std::vector<uint8_t> packed(data.begin(), data.end());
        packed.resize((packed.size() + 15) & ~size_t(15), 0);
        if (k <= aes7::MaxRounds || k == aes7::NoHash) {
            aes7::Password pw(password);
            aes7::Cbc(aes7::derive(pw, p), iv).encrypt(packed.data(), packed.size());
        }
        wr::FolderRecord f;
        wr::CoderRecord a, c;
        a.method = sgcl::compress::detail::sevenzip_format::Aes;
        a.props = aes7::props(k, salt, iv);
        c.method = sgcl::compress::detail::sevenzip_format::Copy;
        f.coders = {a, c};
        f.coder_sizes = {data.size(), data.size()};
        f.unpacked = data.size();
        f.packed = packed.size();
        f.sizes.push_back(data.size());
        f.crcs.push_back(sgcl::hash::crc32::of(compress_test::bytes(data)));
        wr::FileRecord r;
        r.name = u"data.bin";
        r.stream = true;
        r.size = data.size();
        r.crc = f.crcs[0];
        r.attributes = 0x8020 | (0100644u << 16);
        std::vector<uint8_t> out(32, 0);
        out.insert(out.end(), packed.begin(), packed.end());
        auto plain = wr::header({f}, {r});
        std::vector<uint8_t> hp, he;
        wr::packed_header(plain, packed.size(), hp, he);
        out.insert(out.end(), hp.begin(), hp.end());
        out.insert(out.end(), he.begin(), he.end());
        auto sig = wr::signature(packed.size() + hp.size(), he);
        std::copy(sig.begin(), sig.end(), out.begin());
        return std::string(out.begin(), out.end());
    };
    std::string data = repo_text(1000);
    auto nohash = make(aes7::NoHash, data, "pw");
    auto a = sevenzip::archive::from(compress_test::bytes(nohash), with_password("pw"));
    ASSERT_TRUE(a);
    auto d = a->read("data.bin");
    ASSERT_TRUE(d) << d.error().message();
    EXPECT_EQ(text_of(*d), data);
    if (!seven_zip().empty()) {
        scratch_dir scratch("sgcl-7zaesk");
        put(scratch.path() / "nohash.7z", nohash);
        auto x = run_7zz(scratch.path(), "x -ppw nohash.7z");
        EXPECT_EQ(x.code, 0) << x.out;
        EXPECT_EQ(slurp(scratch.path() / "data.bin"), data);
    }
    auto past = make(25, data, "pw");
    auto b = sevenzip::archive::from(compress_test::bytes(past), with_password("pw"));
    ASSERT_TRUE(b);
    auto start = std::chrono::steady_clock::now();
    auto e = b->read("data.bin");
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().code(), sgcl::compress::errc::too_large);
    EXPECT_NE(std::string(e.error().message().view()).find("2^25"), std::string::npos) << e.error().message();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(500));
}

// The password's UTF-16 is laid out in one block reserved for the whole
// of it: the vector never grows, so no block that held a part of the
// password is freed without being wiped (a 4-byte UTF-8 character is a
// surrogate pair, four bytes: two per UTF-8 byte at most)
TEST(SevenZipAes_Tests, APasswordIsLaidOutInOneBlock) {
    namespace aes7 = sgcl::compress::detail::sevenzip_aes;
    for (std::string utf8 : {std::string("Secret"), std::string("za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 \xF0\x9F\x98\x80"),
                             std::string(1000, 'x'), std::string("\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC")}) {
        aes7::Password password(utf8);
        EXPECT_EQ(password.bytes().capacity(), 2 * utf8.size()) << utf8;
        EXPECT_LE(password.bytes().size(), password.bytes().capacity());
    }
}

// A packed header's streams read short (the file shrank between the
// size taken and the read): what the asynchronous open decodes is the
// bytes it got, as the stream they are, and the decoder stops at their
// end with an error; with all of them it decodes. The Opener's steps as
// async_open takes them, over memory
TEST(SevenZip_Tests, APackedHeaderReadShortEndsWithAnError) {
    sgcl::io::buffer b;
    {
        sevenzip::writer w(b);
        for (int i = 0; i < 40; ++i) {
            sgcl::io::writer e = w.create(sgcl::string("file-" + std::to_string(i) + ".txt"));
            ASSERT_TRUE(e.write(compress_test::bytes(std::string(100 + i, char('a' + i % 26)))));
        }
        ASSERT_TRUE(w.close());
    }
    auto data = b.data();
    auto bytes = reinterpret_cast<const uint8_t*>(data.data());
    auto run = [&](size_t keep, bool& decoded) {
        sevenzip::detail::Opener o(data.size(), sgcl::compress::limits{});
        ASSERT_TRUE(o.signature(bytes, data.size()));
        ASSERT_FALSE(o.empty());
        bool plain = false;
        ASSERT_TRUE(o.header(bytes + o.header_offset(), size_t(o.header_size()), plain));
        ASSERT_FALSE(plain) << "the writer packs its header";
        const auto& p = o.packed();
        uint64_t first = p.pack_offsets.empty() ? 0 : p.pack_offsets.front();
        uint64_t total = 0;
        for (auto v : p.pack_sizes) {
            total += v;
        }
        ASSERT_GT(total, 4u);
        size_t got = keep == size_t(-1) ? size_t(total) : std::min(keep, size_t(total));
        sgcl::compress::detail::Source m;
        m.memory = sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(bytes + first), got);
        m.size = got;
        decoded = o.decode(m, plain, first) && plain;
        if (!decoded) {
            EXPECT_TRUE(o.failure().has_value());
        }
    };
    bool whole = false, half = false, none = false;
    run(size_t(-1), whole);
    run(10, half);
    run(0, none);
    EXPECT_TRUE(whole);
    EXPECT_FALSE(half);
    EXPECT_FALSE(none);
}
