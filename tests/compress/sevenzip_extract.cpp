//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// sevenzip::extract and archive::open with a password in the options, and
// their tasks. An archive written here
// (with and without a password, directories, modes, times, a link) extracted
// and compared file by file; a wrong password and none; names and link
// targets that would leave the directory refused with nothing written; the
// password read when the call is made and not after.
#include "common.h"

#include "sgcl/crypto/secret.h"

#include <sys/stat.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

using namespace compress_test;
namespace sevenzip = sgcl::compress::sevenzip;
namespace fs = std::filesystem;

namespace {
    fs::path scratch(const std::string& name) {
        auto d = fs::temp_directory_path() / "sgcl_sevenzip_extract" / name;
        fs::remove_all(d);
        fs::create_directories(d.parent_path());
        return d;
    }

    std::string contents_of(const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    sevenzip::options with_password(const char* p) {
        sevenzip::options o;
        o.password = sgcl::string(p);
        return o;
    }

    // An archive of a tree: two files (one of mode 0600 and a time), a
    // directory, an empty file, a link to a file beside it
    std::string archive_of(const fs::path& where, const sevenzip::options& o) {
        const std::string path = (where.parent_path() / (where.filename().string() + ".7z")).string();
        sevenzip::writer w(sgcl::string(path), o);
        sevenzip::entry_info mode;
        mode.mode = sgcl::io::permissions(0600);
        mode.modified = sgcl::time::datetime::from_unix_nano(int64_t(1700000000) * 1000000000);
        w.add("top.txt", compress_test::bytes(std::string(7000, 't')), mode);
        w.add_directory("dir/inner");
        w.add("dir/file.bin", compress_test::bytes(std::string("\x01\x02\x03", 3)));
        w.add("dir/empty", compress_test::bytes(std::string()));
        sevenzip::entry_info link;
        link.symlink = true;
        w.add("dir/link", compress_test::bytes(std::string("file.bin")), link);
        EXPECT_TRUE(w.close());
        return path;
    }

    void check_tree(const fs::path& out) {
        EXPECT_EQ(contents_of(out / "top.txt"), std::string(7000, 't'));
        EXPECT_EQ(contents_of(out / "dir/file.bin"), std::string("\x01\x02\x03", 3));
        EXPECT_TRUE(fs::is_directory(out / "dir/inner"));
        EXPECT_TRUE(fs::exists(out / "dir/empty"));
        EXPECT_EQ(fs::file_size(out / "dir/empty"), 0u);
        EXPECT_TRUE(fs::is_symlink(out / "dir/link"));
        EXPECT_EQ(fs::read_symlink(out / "dir/link").string(), "file.bin");
        struct ::stat st;
        ASSERT_EQ(::stat((out / "top.txt").c_str(), &st), 0);
        EXPECT_EQ(st.st_mode & 0777, 0600u);
        EXPECT_EQ(int64_t(st.st_mtime), int64_t(1700000000));
    }

    sgcl::compress::errc code_of(const sgcl::expected<void, sgcl::compress::error>& r) {
        return r ? sgcl::compress::errc{} : r.error().code();
    }
}

TEST(SevenZipExtract_Tests, ATreeWithAndWithoutAPassword) {
    const auto plain = scratch("plain");
    const std::string a = archive_of(plain, {});
    auto r = sevenzip::extract(sgcl::string(a), sgcl::string((plain / "out").string()));
    ASSERT_TRUE(r) << r.error().message();
    check_tree(plain / "out");

    const auto enc = scratch("encrypted");
    const std::string b = archive_of(enc, with_password("correct horse"));
    // the password as a literal, a string, a secret_bytes
    auto x = sevenzip::extract(sgcl::string(b), sgcl::string((enc / "out1").string()), sevenzip::options{.password = "correct horse"});
    ASSERT_TRUE(x) << x.error().message();
    check_tree(enc / "out1");
    auto y = sevenzip::extract(sgcl::string(b), sgcl::string((enc / "out2").string()), sevenzip::options{.password = sgcl::string("correct horse")});
    ASSERT_TRUE(y) << y.error().message();
    sgcl::crypto::secret_bytes secret(13);
    std::memcpy(secret.as_slice().data(), "correct horse", 13);
    auto z = sevenzip::extract(sgcl::string(b), sgcl::string((enc / "out3").string()), sevenzip::options{.password = secret});
    ASSERT_TRUE(z) << z.error().message();
    check_tree(enc / "out3");
    // an archive opened, read by name
    auto opened = sevenzip::archive::open(sgcl::string(b), sevenzip::options{.password = secret});
    ASSERT_TRUE(opened);
    auto data = opened->read("dir/file.bin");
    ASSERT_TRUE(data);
    EXPECT_EQ(data->size(), 3u);
}

TEST(SevenZipExtract_Tests, AWrongPasswordAndNone) {
    const auto enc = scratch("wrong");
    const std::string b = archive_of(enc, with_password("correct horse"));
    EXPECT_EQ(code_of(sevenzip::extract(sgcl::string(b), sgcl::string((enc / "out").string()), sevenzip::options{.password = "battery staple"})),
              sgcl::compress::errc::wrong_password);
    EXPECT_EQ(code_of(sevenzip::extract(sgcl::string(b), sgcl::string((enc / "none").string()))),
              sgcl::compress::errc::password_required);
    // the header plain, the data encrypted: the error at the first file
    auto o = with_password("correct horse");
    o.encrypt_header = false;
    const auto half = scratch("half");
    const std::string c = archive_of(half, o);
    EXPECT_EQ(code_of(sevenzip::extract(sgcl::string(c), sgcl::string((half / "out").string()))),
              sgcl::compress::errc::password_required);
    EXPECT_EQ(code_of(sevenzip::extract(sgcl::string(c), sgcl::string((half / "out2").string()), sevenzip::options{.password = "nope"})),
              sgcl::compress::errc::wrong_password);
}

TEST(SevenZipExtract_Tests, NamesAndLinksThatLeaveTheDirectory) {
    for (const char* bad : {"../evil.txt", "a/../../evil.txt", "/abs/evil.txt"}) {
        const auto d = scratch("names");
        const std::string path = (d.parent_path() / "names.7z").string();
        {
            sevenzip::writer w{sgcl::string(path)};
            w.add("fine.txt", compress_test::bytes(std::string("fine")));
            w.add(sgcl::string(bad), compress_test::bytes(std::string("evil")));
            ASSERT_TRUE(w.close()) << bad;
        }
        auto r = sevenzip::extract(sgcl::string(path), sgcl::string((d / "out").string()));
        EXPECT_EQ(code_of(r), sgcl::compress::errc::insecure_path) << bad;
        EXPECT_FALSE(fs::exists(d / "out/fine.txt")) << bad << ": nothing written";
        EXPECT_FALSE(fs::exists(d / "evil.txt")) << bad;
    }
    // a link out: refused where it comes; a link inside through ".." is fine
    for (auto [target, ok] : {std::pair{"../../outside", false}, std::pair{"/etc/passwd", false}, std::pair{"../top.txt", true}}) {
        const auto d = scratch("links");
        const std::string path = (d.parent_path() / "links.7z").string();
        {
            sevenzip::writer w{sgcl::string(path)};
            w.add("top.txt", compress_test::bytes(std::string("top")));
            sevenzip::entry_info link;
            link.symlink = true;
            w.add("dir/link", compress_test::bytes(std::string(target)), link);
            ASSERT_TRUE(w.close());
        }
        auto r = sevenzip::extract(sgcl::string(path), sgcl::string((d / "out").string()));
        if (ok) {
            EXPECT_TRUE(r) << target;
            EXPECT_EQ(contents_of(d / "out/dir/link"), "top");
        } else {
            EXPECT_EQ(code_of(r), sgcl::compress::errc::insecure_path) << target;
            EXPECT_FALSE(fs::is_symlink(d / "out/dir/link")) << target;
        }
    }
}

TEST(SevenZipExtract_Tests, InATaskThePasswordReadAtTheCall) {
    const auto enc = scratch("task");
    const std::string b = archive_of(enc, with_password("correct horse"));
    std::string pw = "correct horse";
    // the tasks made, then the caller's bytes overwritten: the keys were made at the call
    auto extracting = sevenzip::async_extract(sgcl::string(b), sgcl::string((enc / "out").string()), sevenzip::options{.password = std::string_view(pw)});
    auto opening = sevenzip::archive::async_open(sgcl::string(b), sevenzip::options{.password = std::string_view(pw)});
    std::fill(pw.begin(), pw.end(), 'x');
    auto r = sgcl::async::spawn(std::move(extracting)).wait();
    ASSERT_TRUE(r) << r.error().message();
    check_tree(enc / "out");
    auto a = sgcl::async::spawn(std::move(opening)).wait();
    ASSERT_TRUE(a) << a.error().message();
    EXPECT_GE(a->entries().size(), 5u);
}

TEST(SevenZipExtract_Tests, MaxSizeFromTheOptions) {
    const auto d = scratch("max");
    const std::string a = archive_of(d, {});
    sevenzip::options small;
    small.limit.max_size = 2;
    EXPECT_EQ(code_of(sevenzip::extract(sgcl::string(a), sgcl::string((d / "out").string()), small)), sgcl::compress::errc::too_large);
    EXPECT_FALSE(fs::exists(d / "out/dir/file.bin")) << "nothing written";
    sevenzip::options none;
    none.limit.max_size = 0;
    EXPECT_TRUE(sevenzip::extract(sgcl::string(a), sgcl::string((d / "all").string()), none));
}
