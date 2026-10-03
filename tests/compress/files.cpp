//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The archives and gzip on files, plain functions with options (DESIGN
// 325): tar, zip and sevenzip's extract and create, gzip::compress_file and
// decompress_file. The oracles are the system's
// tools, both ways: bsdtar for tar (.tar, .tar.gz, .tar.xz, and .tar.bz2
// to read), Info-ZIP's zip and unzip, 7-Zip's 7zz, gzip(1). A tree of
// directories, files of several sizes and modes and a symbolic link is
// packed by us and unpacked by the tool, and packed by the tool and
// unpacked by us, and the trees compared: the names, the kinds, the bytes,
// the modes, the links' targets. Then what no tool shows: a name or a
// link that leaves the directory refused with nothing written, max_size,
// keep = false, the async forms.
#include "common.h"

#include <map>
#include <thread>

using namespace compress_test;

namespace {
    namespace io = sgcl::io;
    using sgcl::string;

    struct Node {
        char kind;   // d, f, l
        unsigned mode;
        std::string data;   // a file's bytes, a link's target

        bool operator==(const Node&) const = default;
    };

    // The tree under a directory: name -> what it is
    std::map<std::string, Node> tree_of(const std::string& root) {
        std::map<std::string, Node> out;
        namespace fs = std::filesystem;
        for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it) {
            auto name = fs::relative(it->path(), root).string();
            auto st = fs::symlink_status(it->path());
            unsigned mode = unsigned(st.permissions()) & 0777;
            if (fs::is_symlink(st)) {
                out[name] = Node{'l', 0, fs::read_symlink(it->path()).string()};
            } else if (fs::is_directory(st)) {
                out[name] = Node{'d', mode, {}};
            } else {
                std::ifstream in(it->path(), std::ios::binary);
                std::stringstream ss;
                ss << in.rdbuf();
                out[name] = Node{'f', mode, ss.str()};
            }
        }
        return out;
    }

    void write_file(const std::string& path, const std::string& data, unsigned mode = 0644) {
        std::ofstream(path, std::ios::binary) << data;
        std::filesystem::permissions(path, std::filesystem::perms(mode));
    }

    // A fresh directory of the test's own, removed at the end
    struct Scratch {
        std::string dir;

        Scratch() {
            dir = (std::filesystem::temp_directory_path() / ("sgcl_compress_files_" + std::to_string(::getpid()) + "_" + std::to_string(next()))).string();
            std::filesystem::remove_all(dir);
            std::filesystem::create_directories(dir);
        }

        ~Scratch() {
            std::filesystem::remove_all(dir);
        }

        static int next() {
            static std::atomic<int> n{0};
            return n++;
        }

        std::string operator/(const std::string& name) const {
            return dir + "/" + name;
        }
    };

    // The tree the tests pack: nested directories, an empty one, files of
    // 0, a few and a hundred thousand bytes, three modes, a link
    std::string make_source(const Scratch& s) {
        std::string src = s / "src";
        std::filesystem::create_directories(src + "/sub/deep");
        std::filesystem::create_directories(src + "/empty");
        write_file(src + "/a.txt", "hello\n");
        write_file(src + "/zero", "");
        std::string big;
        std::mt19937 rng(7);
        for (int i = 0; i < 100000; ++i) {
            big += char('a' + rng() % 7);
        }
        write_file(src + "/sub/big.txt", big, 0600);
        write_file(src + "/sub/run.sh", "#!/bin/sh\necho hi\n", 0755);
        write_file(src + "/sub/deep/x.bin", std::string("\0\1\2\3", 4));
        std::filesystem::create_symlink("../a.txt", src + "/sub/link");
        return src;
    }

    bool have(const char* tool) {
        return std::system(("command -v " + std::string(tool) + " > /dev/null 2>&1").c_str()) == 0;
    }

    int run(const std::string& command) {
        return std::system(("COPYFILE_DISABLE=1 " + command + " > /dev/null 2>&1").c_str());
    }

    std::string q(const std::string& path) {
        return "'" + path + "'";
    }
}


TEST(CompressFiles_Tests, TarBothWaysWithBsdtar) {
    if (!have("tar")) {
        GTEST_SKIP() << "no tar";
    }
    for (std::string ext : {".tar", ".tar.gz", ".tgz", ".tar.xz"}) {
        Scratch s;
        auto src = make_source(s);
        auto want = tree_of(src);
        // ours, unpacked by bsdtar
        ASSERT_TRUE(compress::tar::create(string(src), string(s / ("ours" + ext)))) << ext;
        std::filesystem::create_directories(s / "by_tool");
        ASSERT_EQ(run("tar -xf " + q(s / ("ours" + ext)) + " -C " + q(s / "by_tool")), 0) << ext;
        EXPECT_EQ(tree_of(s / "by_tool"), want) << ext;
        // bsdtar's, unpacked by us
        std::string flag = ext == ".tar" ? "" : ext == ".tar.xz" ? "J" : "z";
        ASSERT_EQ(run("tar -c" + flag + "f " + q(s / ("tool" + ext)) + " -C " + q(src) + " ."), 0) << ext;
        auto r = compress::tar::extract(string(s / ("tool" + ext)), string(s / "by_us"));
        ASSERT_TRUE(r) << ext << ": " << std::string_view(r.error().message());
        EXPECT_EQ(tree_of(s / "by_us"), want) << ext;
    }
    // the level in the options: 1 against 9 on the same tree
    Scratch s;
    auto src = make_source(s);
    ASSERT_TRUE(compress::tar::create(string(src), string(s / "one.tar.gz"), {.level = 1}));
    ASSERT_TRUE(compress::tar::create(string(src), string(s / "nine.tar.gz"), {.level = 9}));
    EXPECT_GT(std::filesystem::file_size(s / "one.tar.gz"), std::filesystem::file_size(s / "nine.tar.gz"));
}

TEST(CompressFiles_Tests, TarReadsBzip2) {
    if (!have("tar")) {
        GTEST_SKIP() << "no tar";
    }
    Scratch s;
    auto src = make_source(s);
    ASSERT_EQ(run("tar -cjf " + q(s / "t.tar.bz2") + " -C " + q(src) + " ."), 0);
    ASSERT_TRUE(compress::tar::extract(string(s / "t.tar.bz2"), string(s / "out")));
    EXPECT_EQ(tree_of(s / "out"), tree_of(src));
    auto w = compress::tar::create(string(src), string(s / "made.tar.bz2"));
    ASSERT_FALSE(w);
    EXPECT_EQ(w.error().code(), compress::errc::unsupported);
    EXPECT_FALSE(std::filesystem::exists(s / "made.tar.bz2"));
}

// A name that leaves the directory, or a link whose target does, is
// refused before anything is written
TEST(CompressFiles_Tests, TarInsecurePathsWriteNothing) {
    Scratch s;
    auto tar_with = [&](const std::string& path, const sgcl::vector<compress::tar::entry>& entries) {
        auto f = io::create(string(path));
        ASSERT_TRUE(f);
        compress::tar::writer w(*f);
        for (auto& e : entries) {
            ASSERT_TRUE(w.write_header(e));
            if (e.size) {
                std::string data(e.size, 'x');
                ASSERT_TRUE(w.write(sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size())));
            }
        }
        ASSERT_TRUE(w.close());
        ASSERT_TRUE(f->close());
    };
    compress::tar::entry ok;
    ok.name = "fine.txt";
    ok.size = 3;
    ok.mode = io::permissions(0644);
    compress::tar::entry up = ok;
    up.name = "../evil.txt";
    tar_with(s / "up.tar", sgcl::vector<compress::tar::entry>{ok, up});
    auto r = compress::tar::extract(string(s / "up.tar"), string(s / "out1"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::insecure_path);
    EXPECT_FALSE(std::filesystem::exists(s / "out1/fine.txt"));
    EXPECT_FALSE(std::filesystem::exists(s / "evil.txt"));

    compress::tar::entry link;
    link.name = "sub/link";
    link.type = compress::tar::kind::symlink;
    link.link_name = "../../outside";
    tar_with(s / "link.tar", sgcl::vector<compress::tar::entry>{ok, link});
    r = compress::tar::extract(string(s / "link.tar"), string(s / "out2"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::insecure_path);
    EXPECT_FALSE(std::filesystem::exists(s / "out2/fine.txt"));
}

TEST(CompressFiles_Tests, TarMaxSize) {
    Scratch s;
    auto src = make_source(s);
    ASSERT_TRUE(compress::tar::create(string(src), string(s / "t.tar.gz")));
    auto r = compress::tar::extract(string(s / "t.tar.gz"), string(s / "a"), {.max_size = 50000});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    EXPECT_FALSE(std::filesystem::exists(s / "a/a.txt"));
    EXPECT_TRUE(compress::tar::extract(string(s / "t.tar.gz"), string(s / "c"), {.max_size = 200000}));
    EXPECT_TRUE(compress::tar::extract(string(s / "t.tar.gz"), string(s / "d"), {.max_size = 0}));   // 0: no bound
}

TEST(CompressFiles_Tests, TarAsyncForms) {
    Scratch s;
    auto src = make_source(s);
    auto want = tree_of(src);
    auto t = [](string src, string dir) -> sgcl::async::task<bool> {
        bool ok = true;
        ok = ok && co_await compress::tar::async_create(src, dir + "/t.tar.xz", {.level = 9});
        ok = ok && co_await compress::tar::async_extract(dir + "/t.tar.xz", dir + "/t");
        co_return ok;
    }(string(src), string(s.dir));
    ASSERT_TRUE(t.wait());
    EXPECT_EQ(tree_of(s / "t"), want);
}

TEST(CompressFiles_Tests, ZipBothWaysWithInfoZip) {
    if (!have("zip") || !have("unzip")) {
        GTEST_SKIP() << "no zip or unzip";
    }
    Scratch s;
    auto src = make_source(s);
    auto want = tree_of(src);
    ASSERT_TRUE(compress::zip::create(string(src), string(s / "ours.zip")));
    ASSERT_EQ(run("unzip -q " + q(s / "ours.zip") + " -d " + q(s / "by_tool")), 0);
    EXPECT_EQ(tree_of(s / "by_tool"), want);
    ASSERT_EQ(run("cd " + q(src) + " && zip -q -r -y " + q(s / "tool.zip") + " ."), 0);
    auto r = compress::zip::extract(string(s / "tool.zip"), string(s / "by_us"));
    ASSERT_TRUE(r) << std::string_view(r.error().message());
    EXPECT_EQ(tree_of(s / "by_us"), want);
    // stored, not deflated: the same tree, a larger file
    ASSERT_TRUE(compress::zip::create(string(src), string(s / "stored.zip"), {.method = compress::zip::method::store}));
    EXPECT_GT(std::filesystem::file_size(s / "stored.zip"), std::filesystem::file_size(s / "ours.zip"));
    ASSERT_EQ(run("unzip -q " + q(s / "stored.zip") + " -d " + q(s / "stored")), 0);
    EXPECT_EQ(tree_of(s / "stored"), want);
}

TEST(CompressFiles_Tests, ZipInsecurePathWritesNothing) {
    Scratch s;
    {
        auto f = io::create(string(s / "up.zip"));
        ASSERT_TRUE(f);
        compress::zip::writer w(*f);
        ASSERT_TRUE(w.add("fine.txt", "abc"));
        ASSERT_TRUE(w.add("../evil.txt", "abc"));
        ASSERT_TRUE(w.close());
        ASSERT_TRUE(f->close());
    }
    auto r = compress::zip::extract(string(s / "up.zip"), string(s / "out"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::insecure_path);
    EXPECT_FALSE(std::filesystem::exists(s / "out/fine.txt"));
}

TEST(CompressFiles_Tests, ZipMaxSizeAndAsync) {
    Scratch s;
    auto src = make_source(s);
    auto want = tree_of(src);
    ASSERT_TRUE(compress::zip::create(string(src), string(s / "t.zip")));
    auto r = compress::zip::extract(string(s / "t.zip"), string(s / "b"), {.max_size = 50000});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    EXPECT_FALSE(std::filesystem::exists(s / "b/a.txt"));
    EXPECT_TRUE(compress::zip::extract(string(s / "t.zip"), string(s / "d"), {.max_size = 0}));   // 0: no bound
    auto t = [](string src, string dir) -> sgcl::async::task<bool> {
        bool ok = true;
        ok = ok && co_await compress::zip::async_create(src, dir + "/z.zip", {.method = compress::zip::method::store});
        ok = ok && co_await compress::zip::async_extract(dir + "/z.zip", dir + "/z");
        co_return ok;
    }(string(src), string(s.dir));
    ASSERT_TRUE(t.wait());
    EXPECT_EQ(tree_of(s / "z"), want);
}

TEST(CompressFiles_Tests, SevenZipBothWaysWith7zz) {
    if (!have("7zz")) {
        GTEST_SKIP() << "no 7zz";
    }
    Scratch s;
    auto src = make_source(s);
    auto want = tree_of(src);
    ASSERT_TRUE(compress::sevenzip::create(string(src), string(s / "ours.7z")));
    ASSERT_EQ(run("7zz x -snl -snld " + q(s / "ours.7z") + " -o" + q(s / "by_tool")), 0);
    EXPECT_EQ(tree_of(s / "by_tool"), want);
    ASSERT_EQ(run("cd " + q(src) + " && 7zz a -snl " + q(s / "tool.7z") + " ."), 0);
    auto r = compress::sevenzip::extract(string(s / "tool.7z"), string(s / "by_us"));
    ASSERT_TRUE(r) << std::string_view(r.error().message());
    EXPECT_EQ(tree_of(s / "by_us"), want);
    ASSERT_TRUE(compress::sevenzip::create(string(src), string(s / "nine.7z"), {.level = 9, .solid = false}));
    ASSERT_EQ(run("7zz t " + q(s / "nine.7z")), 0);
    // a password: the whole tree encrypted, the names too
    ASSERT_TRUE(compress::sevenzip::create(string(src), string(s / "secret.7z"), {.password = "correct horse"}));
    ASSERT_EQ(run("7zz t -pcorrect\\ horse " + q(s / "secret.7z")), 0);
    ASSERT_TRUE(compress::sevenzip::extract(string(s / "secret.7z"), string(s / "opened"), {.password = "correct horse"}));
    EXPECT_EQ(tree_of(s / "opened"), want);
}

TEST(CompressFiles_Tests, SevenZipAsyncForms) {
    Scratch s;
    auto src = make_source(s);
    auto want = tree_of(src);
    auto t = [](string src, string dir) -> sgcl::async::task<bool> {
        bool ok = true;
        ok = ok && co_await compress::sevenzip::async_create(src, dir + "/s.7z", {.level = 1});
        ok = ok && co_await compress::sevenzip::async_extract(dir + "/s.7z", dir + "/s");
        // a password: the key made at the call, the caller's bytes overwritten before the task runs
        std::string pw = "pw";
        auto creating = compress::sevenzip::async_create(src, dir + "/p.7z", {.password = std::string_view(pw)});
        std::fill(pw.begin(), pw.end(), 'x');
        ok = ok && co_await std::move(creating);
        ok = ok && co_await compress::sevenzip::async_extract(dir + "/p.7z", dir + "/p", {.password = "pw"});
        co_return ok;
    }(string(src), string(s.dir));
    ASSERT_TRUE(t.wait());
    EXPECT_EQ(tree_of(s / "s"), want);
    EXPECT_EQ(tree_of(s / "p"), want);
}

TEST(CompressFiles_Tests, GzipFilesWithGzip) {
    if (!have("gzip")) {
        GTEST_SKIP() << "no gzip";
    }
    Scratch s;
    std::string text;
    for (int i = 0; i < 5000; ++i) {
        text += "line " + std::to_string(i) + "\n";
    }
    write_file(s / "log.txt", text, 0640);
    ASSERT_TRUE(compress::gzip::compress_file(string(s / "log.txt")));
    EXPECT_TRUE(std::filesystem::exists(s / "log.txt"));   // kept
    EXPECT_EQ(unsigned(std::filesystem::status(s / "log.txt.gz").permissions()) & 0777, 0640u);
    ASSERT_EQ(std::system(("gzip -dc " + q(s / "log.txt.gz") + " > " + q(s / "back.txt")).c_str()), 0);
    EXPECT_EQ(tree_of(s.dir)["back.txt"].data, text);
    // the name in the header, as gzip -l shows it
    ASSERT_EQ(std::system(("gzip -lN " + q(s / "log.txt.gz") + " | grep -q 'log.txt$'").c_str()), 0);
    // gzip's, decompressed by us
    write_file(s / "g.txt", text);
    ASSERT_EQ(run("gzip -k -9 " + q(s / "g.txt")), 0);
    std::filesystem::remove(s / "g.txt");
    ASSERT_TRUE(compress::gzip::decompress_file(string(s / "g.txt.gz")));
    EXPECT_EQ(tree_of(s.dir)["g.txt"].data, text);
    EXPECT_TRUE(std::filesystem::exists(s / "g.txt.gz"));   // kept
    // keep = false: the original removed once the other is whole
    ASSERT_TRUE(compress::gzip::compress_file(string(s / "g.txt"), {.level = 9, .keep = false}));
    EXPECT_FALSE(std::filesystem::exists(s / "g.txt"));
    ASSERT_TRUE(compress::gzip::decompress_file(string(s / "g.txt.gz"), {.keep = false}));
    EXPECT_FALSE(std::filesystem::exists(s / "g.txt.gz"));
    EXPECT_EQ(tree_of(s.dir)["g.txt"].data, text);
    // a name without .gz; a file that is not gzip, and nothing left of it
    auto bad = compress::gzip::decompress_file(string(s / "g.txt"));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), compress::errc::invalid_argument);
    write_file(s / "junk.gz", "not gzip at all");
    EXPECT_FALSE(compress::gzip::decompress_file(string(s / "junk.gz")));
    EXPECT_FALSE(std::filesystem::exists(s / "junk"));
}

// A .gz whose data is damaged fails where the damage is, with the words
// the reader gives (as gzip::decompress gives them for the same bytes),
// never at offset 0
TEST(CompressFiles_Tests, GzipFileErrorsSayWhere) {
    Scratch s;
    std::string text;
    for (int i = 0; i < 5000; ++i) {
        text += "line " + std::to_string(i) + "\n";
    }
    auto gz = compress::gzip::compress(bytes(text));
    std::string good(reinterpret_cast<const char*>(gz.data()), gz.size());
    std::string crc = good;
    crc[crc.size() - 8] ^= 1;   // the trailer's CRC-32
    std::string data = good;
    data[data.size() / 2] ^= 0x55;   // the middle of the DEFLATE data
    for (auto& damaged : {crc, data}) {
        write_file(s / "d.txt.gz", damaged);
        auto in_memory = compress::gzip::decompress(bytes(damaged));
        ASSERT_FALSE(in_memory);
        auto r = compress::gzip::decompress_file(string(s / "d.txt.gz"));
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), in_memory.error().code());
        EXPECT_GT(r.error().offset(), 0u);
        EXPECT_EQ(r.error().message(), in_memory.error().message());
        EXPECT_FALSE(std::filesystem::exists(s / "d.txt"));
    }
}

namespace {
    // An error that did not come from the data: offset 0, and no place in
    // its message, which is the words alone
    void expect_no_place(const compress::error& e, const std::string& message) {
        EXPECT_EQ(e.offset(), 0u) << message;
        EXPECT_EQ(std::string(e.message().view()), message);
    }

    // The same for a failure of a file: the io error's words after the code's
    void expect_file_error(const compress::error& e) {
        ASSERT_TRUE(e.io_error());
        expect_no_place(e, "input/output error: " + std::string(e.io_error()->message().view()));
    }
}

// A file that does not open or cannot be made, a name refused, a usage
// mistake: no place in the data, so the message is the words alone
// ("input/output error: open missing.zip: ...", not "offset 0: ...");
// an error of the data keeps its offset
TEST(CompressFiles_Tests, ErrorsNotFromTheDataHaveNoPlace) {
    Scratch s;
    const string missing(s / "missing");
    const string nowhere(s / "no-such-directory/out");
    auto z = compress::zip::archive::open(missing + ".zip");
    ASSERT_FALSE(z);
    expect_file_error(z.error());
    EXPECT_NE(z.error(), compress::error(z.error().io_error().value(), 0));   // no place is not offset 0
    auto seven = compress::sevenzip::archive::open(missing + ".7z");
    ASSERT_FALSE(seven);
    expect_file_error(seven.error());
    auto zx = compress::zip::extract(missing + ".zip", string(s / "out"));
    ASSERT_FALSE(zx);
    expect_file_error(zx.error());
    auto tx = compress::tar::extract(missing + ".tar", string(s / "out"));
    ASSERT_FALSE(tx);
    expect_file_error(tx.error());
    auto sx = compress::sevenzip::extract(missing + ".7z", string(s / "out"));
    ASSERT_FALSE(sx);
    expect_file_error(sx.error());
    std::filesystem::create_directories(s / "src");
    write_file(s / "src/a.txt", "a\n");
    auto zc = compress::zip::create(string(s / "src"), nowhere + ".zip");
    ASSERT_FALSE(zc);
    expect_file_error(zc.error());
    auto tc = compress::tar::create(string(s / "src"), nowhere + ".tar");
    ASSERT_FALSE(tc);
    expect_file_error(tc.error());
    auto sc = compress::sevenzip::create(string(s / "src"), nowhere + ".7z");
    ASSERT_FALSE(sc);
    expect_file_error(sc.error());
    auto tb = compress::tar::create(string(s / "src"), string(s / "a.tar.bz2"));
    ASSERT_FALSE(tb);
    expect_no_place(tb.error(), "tar: bzip2 is read, not written: " + (s / "a.tar.bz2"));
    {
        compress::sevenzip::writer w(nowhere + ".7z");
        w.add("a.txt", "a");
        auto r = w.close();
        ASSERT_FALSE(r);
        expect_file_error(r.error());
    }
    // gzip's file functions
    auto gc = compress::gzip::compress_file(missing);
    ASSERT_FALSE(gc);
    expect_file_error(gc.error());
    auto gd = compress::gzip::decompress_file(missing + ".gz");
    ASSERT_FALSE(gd);
    expect_file_error(gd.error());
    auto gn = compress::gzip::decompress_file(string(s / "src/a.txt"));
    ASSERT_FALSE(gn);
    expect_no_place(gn.error(), "gzip: a name that does not end in .gz: " + (s / "src/a.txt"));
    // what the archive does not hold, what the caller's limit refuses
    write_file(s / "big.txt", std::string(100, 'x'));
    ASSERT_TRUE(compress::zip::create(string(s / "src"), string(s / "a.zip")));
    auto za = compress::zip::archive::open(string(s / "a.zip"));
    ASSERT_TRUE(za);
    auto none = za->read(string("none.txt"));
    ASSERT_FALSE(none);
    expect_no_place(none.error(), "zip: no entry none.txt");
    ASSERT_TRUE(za->close());
    ASSERT_TRUE(compress::sevenzip::create(string(s / "src"), string(s / "a.7z")));
    auto sa = compress::sevenzip::archive::open(string(s / "a.7z"));
    ASSERT_TRUE(sa);
    auto snone = sa->read(string("none.txt"));
    ASSERT_FALSE(snone);
    expect_no_place(snone.error(), "7z: no entry none.txt");
    auto slimit = sa->read(string("a.txt"), compress::limits{1});
    ASSERT_FALSE(slimit);
    expect_no_place(slimit.error(), "7z: entry a.txt: larger than the limit");
    ASSERT_TRUE(sa->close());
    auto tl = compress::tar::create(string(s / "src"), string(s / "a.tar"));
    ASSERT_TRUE(tl);
    auto tm = compress::tar::extract(string(s / "a.tar"), string(s / "out"), {.max_size = 1});
    ASSERT_FALSE(tm);
    expect_no_place(tm.error(), "tar: the files are larger than max_size");
    // an error of the data keeps its place
    auto bad = compress::zip::archive::from(bytes("not a zip at all"));
    ASSERT_FALSE(bad);
    EXPECT_EQ(std::string(bad.error().message().view()).rfind("offset ", 0), 0u) << bad.error().message();
}

// A writer's mistake, or a failure of what it writes into, is not a place
// in any data: the words alone
TEST(CompressFiles_Tests, WriterMistakesHaveNoPlace) {
    {
        sgcl::io::buffer b;
        compress::zip::writer w(b);
        ASSERT_TRUE(w.add("a.txt", bytes("abc")));
        auto r = w.set_comment(string(std::string(70000, 'c')));
        ASSERT_FALSE(r);
        expect_no_place(r.error(), "zip: comment longer than 65535 bytes");
        expect_no_place(*w.last_error(), "zip: comment longer than 65535 bytes");
    }
    {
        sgcl::io::buffer b;
        compress::zip::writer w(b);
        ASSERT_TRUE(w.close());
        auto r = w.create(string("late.txt"));
        ASSERT_FALSE(r);
        expect_no_place(r.error(), "zip: create after close");
    }
    {
        sgcl::io::buffer b;
        compress::tar::writer w(b);
        compress::tar::entry e;
        e.name = "a.txt";
        e.size = 3;
        ASSERT_TRUE(w.write_header(e));
        ASSERT_TRUE(w.write(bytes("abc")));
        compress::tar::entry nameless;
        auto r = w.write_header(nameless);
        ASSERT_FALSE(r);
        expect_no_place(r.error(), "tar: an entry with no name");
    }
    {
        sgcl::io::buffer b;
        compress::sevenzip::writer w(b);
        w.add("a.txt", bytes("abc"));
        w.add("", bytes("x"));
        auto r = w.close();
        ASSERT_FALSE(r);
        expect_no_place(r.error(), "7z: an entry name that is empty, has a NUL or is not UTF-8");
    }
}

// A source file whose read fails stops the create with the read's error,
// without a place, and the archive is removed: zip, tar and 7z alike. The
// failure: b.txt, a file when the tree is listed, is made a directory
// once the archive exists (the listing is over by then), while the
// encoder is still at a.bin, random bytes that keep it ~150 ms (eight
// megabytes for deflate, one for xz and LZMA2); b.txt then opens and
// its read fails (EISDIR). 7z's create ignored that read and wrote the
// archive without b.txt's bytes, with no error
TEST(CompressFiles_Tests, CreateStopsAtAFailedRead) {
    namespace fs = std::filesystem;
    Scratch s;
    std::string noise(size_t(8) << 20, '\0');
    std::mt19937 rng(11);
    for (auto& c : noise) {
        c = char(rng());
    }
    auto attempt = [&](const std::string& name, size_t size, auto create) {
        SCOPED_TRACE(name);
        const std::string src = s / ("src-" + name);
        const std::string archive = s / name;
        fs::create_directories(src);
        write_file(src + "/a.bin", noise.substr(0, size));
        write_file(src + "/b.txt", "b\n");
        std::atomic<bool> done{false};
        std::atomic<bool> swapped{false};
        std::thread swap([&] {
            while (!done.load() && !fs::exists(archive)) {
            }
            std::error_code ec;
            if (!done.load() && fs::remove(src + "/b.txt", ec) && fs::create_directory(src + "/b.txt", ec)) {
                swapped = true;
            }
        });
        auto r = create(string(src), string(archive));
        done = true;
        swap.join();
        ASSERT_TRUE(swapped.load());
        ASSERT_FALSE(r);
        expect_file_error(r.error());
        EXPECT_EQ(r.error().io_error()->code(), std::errc::is_a_directory) << std::string_view(r.error().message());
        EXPECT_FALSE(fs::exists(archive));
    };
    attempt("a.zip", size_t(8) << 20, [](const string& d, const string& a) { return compress::zip::create(d, a); });
    attempt("a.tar.xz", size_t(1) << 20, [](const string& d, const string& a) { return compress::tar::create(d, a); });
    attempt("a.7z", size_t(1) << 20, [](const string& d, const string& a) { return compress::sevenzip::create(d, a); });
}

// A name past ISO 8859-1 is written as the bytes of the file's name, as
// gzip(1) writes it: the same header as gzip's own, gunzip -N restoring
// the name from it; read back, the header's name is the file's, and
// decompress_file makes the file of that name again
TEST(CompressFiles_Tests, GzipFileNamePastLatin1AsGzip) {
    Scratch s;
    const std::string euro = "\xE2\x82\xAC.txt";   // €.txt
    write_file(s / euro, "euro\n");
    auto done = compress::gzip::compress_file(string(s / euro), {.keep = false});
    ASSERT_TRUE(done) << std::string(done.error().message().view());
    EXPECT_FALSE(std::filesystem::exists(s / euro));
    std::string gz = tree_of(s.dir)[euro + ".gz"].data;
    ASSERT_GT(gz.size(), 10u);
    EXPECT_EQ(gz[3] & 8, 8);   // FNAME
    EXPECT_EQ(gz.substr(10, euro.size() + 1), euro + std::string(1, '\0'));
    {
        io::reader file = *io::open(string(s / (euro + ".gz")));
        compress::gzip::reader r(file);
        auto h = r.header();
        ASSERT_TRUE(h);
        EXPECT_EQ(std::string(h->name.view()), euro);
        (void)r.close();
    }
    ASSERT_TRUE(compress::gzip::decompress_file(string(s / (euro + ".gz")), {.keep = false}));
    EXPECT_EQ(tree_of(s.dir)[euro].data, "euro\n");
    if (!have("gzip")) {
        GTEST_SKIP() << "no gzip";
    }
    // gzip's header for the same file names it with the same bytes
    ASSERT_EQ(run("gzip -k " + q(s / euro)), 0);
    std::string theirs = tree_of(s.dir)[euro + ".gz"].data;
    ASSERT_GT(theirs.size(), 10u);
    EXPECT_EQ(theirs.substr(10, euro.size() + 1), euro + std::string(1, '\0'));
    // and gunzip -N takes the name from ours
    std::filesystem::remove(s / (euro + ".gz"));
    ASSERT_TRUE(compress::gzip::compress_file(string(s / euro), {.keep = false}));
    std::filesystem::rename(s / (euro + ".gz"), s / "renamed.gz");
    ASSERT_EQ(run("cd " + q(s.dir) + " && gzip -dN renamed.gz"), 0);
    EXPECT_EQ(tree_of(s.dir)[euro].data, "euro\n");
}

TEST(CompressFiles_Tests, GzipAsyncForms) {
    Scratch s;
    write_file(s / "a.txt", "hello\n");
    auto t = [](string dir) -> sgcl::async::task<bool> {
        bool ok = bool(co_await compress::gzip::async_compress_file(dir + "/a.txt", {.level = 1, .keep = false}));
        ok = ok && co_await compress::gzip::async_decompress_file(dir + "/a.txt.gz");
        co_return ok;
    }(string(s.dir));
    ASSERT_TRUE(t.wait());
    EXPECT_EQ(tree_of(s.dir)["a.txt"].data, "hello\n");
    EXPECT_TRUE(std::filesystem::exists(s / "a.txt.gz"));
}
