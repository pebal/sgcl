//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::sftp against OpenSSH: /usr/bin/sftp in batch mode against the
// module's server (every command it has for a file system, files of
// megabytes both ways, its OpenSSH extensions), and the module's client
// against OpenSSH's sftp-server behind /usr/sbin/sshd run unprivileged on a
// loopback port (every operation, its extensions, large files). Skipped
// where OpenSSH is not installed.
#include "tests/net/ssh/helpers.h"
#include "sgcl/net/sftp.h"

#include <filesystem>
#include <fstream>

using namespace sgcl;
using namespace sgcl_ssh_test;
namespace fsys = std::filesystem;

namespace {
    bool openssh() {
        return have("/usr/sbin/sshd") && have("/usr/bin/sftp") && have("/usr/libexec/sftp-server");
    }

    void random_file(const fsys::path& p, size_t size) {
        std::ofstream out(p, std::ios::binary);
        std::string block(1 << 20, 0);
        uint32_t x = 2463534242u;
        for (size_t at = 0; at < size; at += block.size()) {
            for (auto& c : block) {
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                c = char(x);
            }
            out.write(block.data(), std::streamsize(std::min(block.size(), size - at)));
        }
    }
}

TEST(SftpOpenSsh, SftpAgainstOurServer) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    auto base = temp_dir("sftp_cli");
    auto root = base / "root";
    fsys::create_directories(root);
    copy_key(base, "ed25519");
    random_file(base / "up.bin", 20 * 1024 * 1024 + 7);
    net::ssh::server srv = echo_server();
    sgcl::string r(root.string());
    srv.handle([r](net::ssh::server_session s) {
        if (s.subsystem() == "sftp") {
            (void)net::sftp::serve(s, r);
        }
    });
    LocalServer ls(srv);
    std::ofstream(base / "batch") << "pwd\n"
                                     "mkdir dir\n"
                                     "cd dir\n"
                                     "put " << (base / "up.bin").string() << "\n"
                                     "ls -l\n"
                                     "get up.bin " << (base / "down.bin").string() << "\n"
                                     "rename up.bin moved.bin\n"
                                     "ln -s moved.bin sl\n"
                                     "ln moved.bin hard.bin\n"
                                     "chmod 600 moved.bin\n"
                                     "df\n"
                                     "cd ..\n"
                                     "ls dir\n"
                                     "rm dir/sl\n"
                                     "rm dir/hard.bin\n"
                                     "get /../../dir/moved.bin " << (base / "again.bin").string() << "\n"
                                     "rm dir/moved.bin\n"
                                     "rmdir dir\n"
                                     "ls\n";
    std::string out = shell("cd " + base.string() + " && /usr/bin/sftp -F /dev/null -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=error -i " +
                            (base / "ed25519").string() + " -P " + std::to_string(ls.port) + " -b batch user@127.0.0.1 2>&1; echo rc=$?");
    EXPECT_NE(out.find("rc=0"), std::string::npos) << out;
    EXPECT_NE(out.find("Remote working directory: /"), std::string::npos) << out;
    EXPECT_NE(out.find("moved.bin"), std::string::npos) << out;
    EXPECT_TRUE(read_file((base / "up.bin").string()) == read_file((base / "down.bin").string()));
    EXPECT_TRUE(read_file((base / "up.bin").string()) == read_file((base / "again.bin").string()));
    EXPECT_TRUE(fsys::is_empty(root));
    fsys::remove_all(base);
}

TEST(SftpOpenSsh, OurClientAgainstSftpServer) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    Sshd sshd;
    ASSERT_TRUE(sshd.running);
    auto o = client_options();
    o.user = sgcl::string(user_name());
    auto fs = net::sftp::client::connect(sshd.address(), o);
    ASSERT_TRUE(fs) << text(fs.error().message());
    EXPECT_TRUE(fs->has_extension("posix-rename@openssh.com"));
    EXPECT_TRUE(fs->has_extension("limits@openssh.com"));
    EXPECT_GT(fs->limits().max_read_length, 32768u);
    auto home = fs->home_dir();
    ASSERT_TRUE(home);
    EXPECT_EQ(text(*home), text(*io::getenv(sgcl::string("HOME"))));
    // a directory of the test's own (sftp-server serves the whole file system)
    auto base = temp_dir("sftp_srv");
    const std::string d = fsys::canonical(base).string();
    auto at = [&](const std::string& rel) { return sgcl::string(d + "/" + rel); };
    ASSERT_TRUE(fs->mkdir_all(at("a/b")));
    ASSERT_TRUE(fs->write_file(at("a/f.txt"), slice<const byte>("from the module")));
    EXPECT_EQ(read_file(d + "/a/f.txt"), "from the module");
    EXPECT_EQ(text(*fs->read_text(at("a/f.txt"))), "from the module");
    auto st = fs->stat(at("a/f.txt"));
    ASSERT_TRUE(st);
    EXPECT_EQ(st->size, 15u);
    EXPECT_EQ(st->uid, ::getuid());
    auto f = fs->open(at("a/f.txt"), io::open_flags::read | io::open_flags::write);
    ASSERT_TRUE(f);
    ASSERT_TRUE(f->seek(5));
    vector<byte> buf(3);
    ASSERT_EQ(*f->read(buf.as_slice()), 3u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), 3), "the");
    ASSERT_TRUE(f->write_at(slice<const byte>("FROM"), 0));
    ASSERT_TRUE(f->truncate(8));
    ASSERT_TRUE(f->sync());
    EXPECT_EQ(f->stat()->size, 8u);
    ASSERT_TRUE(f->close());
    EXPECT_EQ(read_file(d + "/a/f.txt"), "FROM the");
    ASSERT_TRUE(fs->symlink(sgcl::string("f.txt"), at("a/l")));
    EXPECT_EQ(fs->read_link(at("a/l")), "f.txt");
    EXPECT_TRUE(fs->lstat(at("a/l"))->is_symlink());
    EXPECT_EQ(fs->real_path(at("a/b/../l")), at("a/f.txt"));
    ASSERT_TRUE(fs->hard_link(at("a/f.txt"), at("a/h")));
    EXPECT_EQ(read_file(d + "/a/h"), "FROM the");
    ASSERT_TRUE(fs->remove(at("a/h")));
    ASSERT_TRUE(fs->write_file(at("a/other"), slice<const byte>("other")));
    ASSERT_TRUE(fs->rename(at("a/other"), at("a/f.txt")));   // posix-rename: replaced
    EXPECT_EQ(read_file(d + "/a/f.txt"), "other");
    ASSERT_TRUE(fs->chmod(at("a/f.txt"), io::permissions(0640)));
    EXPECT_EQ(unsigned(fs->stat(at("a/f.txt"))->mode), 0640u);
    auto when = io::file_time(std::chrono::seconds(1400000000));
    ASSERT_TRUE(fs->set_modified(at("a/f.txt"), when));
    EXPECT_EQ(fs->stat(at("a/f.txt"))->modified, when);
    auto list = fs->read_dir(at("a"));
    ASSERT_TRUE(list);
    EXPECT_EQ(list->size(), 3u);   // b, f.txt, l
    EXPECT_TRUE(fs->stat_fs(sgcl::string(d)));
    EXPECT_TRUE(fs->stat(at("none")).error().is_not_found());
    EXPECT_EQ(fs->mkdir(at("a")).error().code(), net::errc::sftp_failure);
    EXPECT_TRUE(fs->open(sgcl::string("/etc/master.passwd")).error().is_permission());
    // large files both ways
    random_file(base / "big", 30 * 1024 * 1024 + 3);
    auto up = fs->upload(sgcl::string((base / "big").string()), at("a/big.up"));
    ASSERT_TRUE(up) << text(up.error().message());
    auto down = fs->download(at("a/big.up"), sgcl::string((base / "big.down").string()));
    ASSERT_TRUE(down) << text(down.error().message());
    EXPECT_TRUE(read_file((base / "big").string()) == read_file((base / "big.down").string()));
    ASSERT_TRUE(fs->remove_all(at("a")));
    EXPECT_FALSE(fsys::exists(d + "/a"));
    ASSERT_TRUE(fs->close());
    fsys::remove_all(base);
}
