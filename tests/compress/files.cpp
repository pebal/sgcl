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
