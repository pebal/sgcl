//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::sftp's client against its server on the loopback, a temporary
// directory served: every operation at its boundaries, the errors as their
// errno values, the ways out of the root tried on the disk (".." and
// absolute paths, symlinks the client makes and symlinks already there
// pointing out, a hard link to a file outside, a rename out), files of
// megabytes both ways, requests from many tasks and threads at once, a
// read-only server, a closed session.
#include "tests/net/ssh/helpers.h"
#include "sgcl/net/sftp.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace sgcl;
using namespace sgcl_ssh_test;
namespace fsys = std::filesystem;

namespace {
    // A directory served over a server of the module's, and a client
    // connected to it
    struct Served {
        fsys::path root;
        fsys::path outside;
        net::ssh::server srv;
        uint16_t port = 0;
        net::sftp::client fs;

        explicit Served(net::sftp::server_options o = {}, const char* name = "sftp") {
            auto base = temp_dir(name);
            root = base / "root";
            outside = base / "outside";
            fsys::create_directories(root);
            fsys::create_directories(outside);
            std::ofstream(outside / "secret") << "do not read";
            srv = net::ssh::server();
            srv.host_keys = {key("ed25519")};
            srv.no_client_auth = true;
            sgcl::string r(root.string());
            srv.handle([r, o](net::ssh::server_session s) {
                if (s.subsystem() == "sftp") {
                    (void)net::sftp::serve(s, r, o);
                }
            });
            auto l = *net::tcp::listen("127.0.0.1:0");
            port = l.local_endpoint().port();
            async::go(serve_task(srv, l));
            auto opts = client_options();
            opts.keys = {};
            auto c = net::sftp::client::connect(sgcl::string("127.0.0.1:" + std::to_string(port)), opts);
            EXPECT_TRUE(c) << text(c.error().message());
            if (c) {
                fs = *c;
            }
        }

        ~Served() {
            if (fs) {
                (void)fs.close();
            }
            srv.close();
            fsys::remove_all(root.parent_path());
        }

        std::string disk(const std::string& rel) const {
            return read_file((root / rel).string());
        }
    };

    std::string err(const io::error& e) {
        return text(e.message());
    }
}

TEST(SftpLoopback, FilesReadWrittenSeekedAsStreams) {
    Served s;
    auto& fs = s.fs;
    EXPECT_TRUE(fs.write_file(sgcl::string("/a.txt"), slice<const byte>("hello, world")));
    EXPECT_EQ(s.disk("a.txt"), "hello, world");
    EXPECT_EQ(text(*fs.read_text(sgcl::string("a.txt"))), "hello, world");   // relative: under "/"
    auto f = fs.open(sgcl::string("/a.txt"), io::open_flags::read | io::open_flags::write);
    ASSERT_TRUE(f) << err(f.error());
    vector<byte> buf(5);
    ASSERT_EQ(*f->read(buf.as_slice()), 5u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), 5), "hello");
    EXPECT_EQ(*f->seek(7), 7u);
    EXPECT_EQ(text(*f->read_all_text()), "world");
    EXPECT_EQ(*f->read(buf.as_slice()), 0u);   // the end
    EXPECT_EQ(*f->seek(-5, io::seek_from::end), 7u);
    ASSERT_TRUE(f->write(sgcl::string("WORLD")));
    EXPECT_EQ(*f->seek(0, io::seek_from::current), 12u);
    EXPECT_EQ(*f->read_at(buf.as_slice(), 0), 5u);
    ASSERT_TRUE(f->write_at(slice<const byte>("H"), 0));
    EXPECT_EQ(f->stat()->size, 12u);
    ASSERT_TRUE(f->truncate(5));
    ASSERT_TRUE(f->sync());
    ASSERT_TRUE(f->close());
    ASSERT_TRUE(f->close());   // twice: nothing
    EXPECT_TRUE(f->is_closed());
    EXPECT_EQ(f->read(buf.as_slice()).error().code(), io::errc::closed);
    EXPECT_EQ(s.disk("a.txt"), "Hello");
    EXPECT_FALSE(f->seek(-100, io::seek_from::current));
    // create, append, exclusive
    auto c = fs.create(sgcl::string("/c"), io::permissions(0600));
    ASSERT_TRUE(c);
    ASSERT_TRUE(c->write(sgcl::string("one")));
    ASSERT_TRUE(c->close());
    EXPECT_EQ(unsigned(fs.stat(sgcl::string("/c"))->mode) & 0777, 0600u);
    auto app = fs.open(sgcl::string("/c"), io::open_flags::append);
    ASSERT_TRUE(app->write(sgcl::string("two")));
    ASSERT_TRUE(app->close());
    EXPECT_EQ(s.disk("c"), "onetwo");
    auto excl = fs.open(sgcl::string("/c"), io::open_flags::write | io::open_flags::create | io::open_flags::exclusive);
    ASSERT_FALSE(excl);
    EXPECT_EQ(excl.error().code(), net::errc::sftp_failure);
    // a stream of io: copy from a local file
    auto local = s.root.parent_path() / "local";
    std::ofstream(local) << std::string(100000, 'q');
    auto lf = *io::open(sgcl::string(local.string()));
    auto rf = *fs.create(sgcl::string("/copied"));
    EXPECT_EQ(*io::copy(rf, lf), 100000u);
    ASSERT_TRUE(rf.close());
    EXPECT_EQ(s.disk("copied"), std::string(100000, 'q'));
    (void)lf.close();
}

TEST(SftpLoopback, StatsListingsAndTheFileSystem) {
    Served s;
    auto& fs = s.fs;
    ASSERT_TRUE(fs.mkdir(sgcl::string("/dir"), io::permissions(0750)));
    EXPECT_EQ(fs.mkdir(sgcl::string("/dir")).error().code(), net::errc::sftp_failure);   // there already
    EXPECT_TRUE(fs.mkdir(sgcl::string("/no/parent")).error().is_not_found());
    ASSERT_TRUE(fs.mkdir_all(sgcl::string("/a/b/c")));
    ASSERT_TRUE(fs.mkdir_all(sgcl::string("/a/b/c")));   // there: no error
    ASSERT_TRUE(fs.write_file(sgcl::string("/dir/f"), slice<const byte>("12345")));
    EXPECT_EQ(fs.mkdir_all(sgcl::string("/dir/f/x")).error().code(), std::errc::not_a_directory);
    auto st = fs.stat(sgcl::string("/dir/f"));
    ASSERT_TRUE(st);
    EXPECT_EQ(st->name, "f");
    EXPECT_EQ(st->size, 5u);
    EXPECT_TRUE(st->is_regular());
    EXPECT_TRUE(fs.stat(sgcl::string("/dir"))->is_directory());
    EXPECT_EQ(unsigned(fs.stat(sgcl::string("/dir"))->mode) & 0777, 0750u & ~0u);
    EXPECT_TRUE(fs.stat(sgcl::string("/none")).error().is_not_found());
    EXPECT_TRUE(*fs.exists(sgcl::string("/dir/f")));
    EXPECT_FALSE(*fs.exists(sgcl::string("/dir/g")));
    ASSERT_TRUE(fs.symlink(sgcl::string("f"), sgcl::string("/dir/l")));
    EXPECT_TRUE(fs.lstat(sgcl::string("/dir/l"))->is_symlink());
    EXPECT_TRUE(fs.stat(sgcl::string("/dir/l"))->is_regular());
    EXPECT_EQ(fs.read_link(sgcl::string("/dir/l")), "f");
    EXPECT_EQ(fs.real_path(sgcl::string("/dir/./../dir/l")), "/dir/f");
    EXPECT_EQ(fs.real_path(sgcl::string(".")), "/");
    EXPECT_EQ(fs.home_dir(), "/");
    // a listing of many entries (the server answers 100 a time)
    for (int i = 0; i < 250; ++i) {
        std::ofstream(s.root / "dir" / ("n" + std::to_string(i)));
    }
    auto list = fs.read_dir(sgcl::string("/dir"));
    ASSERT_TRUE(list);
    EXPECT_EQ(list->size(), 252u);
    EXPECT_TRUE(fs.read_dir(sgcl::string("/dir/f")).error().is_not_found());   // ENOTDIR as SSH_FX_NO_SUCH_FILE, as OpenSSH answers it
    // attributes
    ASSERT_TRUE(fs.chmod(sgcl::string("/dir/f"), io::permissions(0604)));
    EXPECT_EQ(unsigned(fs.stat(sgcl::string("/dir/f"))->mode), 0604u);
    auto when = io::file_time(std::chrono::seconds(1500000000));
    ASSERT_TRUE(fs.set_modified(sgcl::string("/dir/f"), when));
    EXPECT_EQ(fs.stat(sgcl::string("/dir/f"))->modified, when);
    net::sftp::attributes a;
    a.size = 2;
    ASSERT_TRUE(fs.set_stat(sgcl::string("/dir/f"), a));
    EXPECT_EQ(s.disk("dir/f"), "12");
    auto v = fs.stat_fs(sgcl::string("/"));
    ASSERT_TRUE(v);
    EXPECT_GT(v->block_size, 0u);
    EXPECT_EQ(fs.limits().max_read_length, net::sftp::detail::MaxData);
    EXPECT_TRUE(fs.has_extension("posix-rename@openssh.com"));
    EXPECT_FALSE(fs.has_extension("copy-data"));
}

TEST(SftpLoopback, RenamesRemovesAndLinks) {
    Served s;
    auto& fs = s.fs;
    ASSERT_TRUE(fs.write_file(sgcl::string("/a"), slice<const byte>("a")));
    ASSERT_TRUE(fs.write_file(sgcl::string("/b"), slice<const byte>("b")));
    ASSERT_TRUE(fs.rename(sgcl::string("/a"), sgcl::string("/b")));   // replaced, as io::rename
    EXPECT_EQ(s.disk("b"), "a");
    EXPECT_TRUE(fs.rename(sgcl::string("/a"), sgcl::string("/c")).error().is_not_found());
    ASSERT_TRUE(fs.hard_link(sgcl::string("/b"), sgcl::string("/h")));
    EXPECT_EQ(s.disk("h"), "a");
    ASSERT_TRUE(fs.mkdir_all(sgcl::string("/t/u/v")));
    ASSERT_TRUE(fs.write_file(sgcl::string("/t/u/v/w"), slice<const byte>("w")));
    ASSERT_TRUE(fs.symlink(sgcl::string("/b"), sgcl::string("/t/link")));
    EXPECT_EQ(fs.rmdir(sgcl::string("/t")).error().code(), net::errc::sftp_failure);   // not empty
    ASSERT_TRUE(fs.remove(sgcl::string("/t/link")));                                // the link, not its target
    EXPECT_TRUE(fsys::exists(s.root / "b"));
    ASSERT_TRUE(fs.remove_all(sgcl::string("/t")));
    EXPECT_FALSE(fsys::exists(s.root / "t"));
    ASSERT_TRUE(fs.remove_all(sgcl::string("/t")));   // not there: no error
    ASSERT_TRUE(fs.mkdir(sgcl::string("/empty")));
    ASSERT_TRUE(fs.remove(sgcl::string("/empty")));   // an empty directory too, as io::remove
    EXPECT_TRUE(fs.remove(sgcl::string("/none")).error().is_not_found());
    EXPECT_EQ(fs.rmdir(sgcl::string("/")).error().code(), std::errc::permission_denied);
}

TEST(SftpLoopback, NoWayOutOfTheRoot) {
    Served s;
    auto& fs = s.fs;
    const std::string secret = (s.outside / "secret").string();
    // ".." and absolute paths: the root's own
    EXPECT_TRUE(fs.stat(sgcl::string("/../outside/secret")).error().is_not_found());
    EXPECT_TRUE(fs.stat(sgcl::string("../../outside/secret")).error().is_not_found());
    EXPECT_TRUE(fs.read_file(sgcl::string(secret)).error().is_not_found());
    EXPECT_EQ(fs.real_path(sgcl::string("/../../..")), "/");
    // a symlink the client makes, pointing out: followed within the root
    ASSERT_TRUE(fs.symlink(sgcl::string(secret), sgcl::string("/abs")));
    ASSERT_TRUE(fs.symlink(sgcl::string("../outside/secret"), sgcl::string("/rel")));
    ASSERT_TRUE(fs.symlink(sgcl::string("../.."), sgcl::string("/up")));
    EXPECT_TRUE(fs.read_file(sgcl::string("/abs")).error().is_not_found());
    EXPECT_TRUE(fs.read_file(sgcl::string("/rel")).error().is_not_found());
    EXPECT_TRUE(fs.read_file(sgcl::string("/up/outside/secret")).error().is_not_found());
    EXPECT_EQ(fs.real_path(sgcl::string("/up")), "/");
    // a symlink already there pointing out (made on the server's side)
    fsys::create_symlink(s.outside, s.root / "planted");
    fsys::create_symlink(secret, s.root / "planted_file");
    EXPECT_TRUE(fs.read_file(sgcl::string("/planted/secret")).error().is_not_found());
    EXPECT_TRUE(fs.read_file(sgcl::string("/planted_file")).error().is_not_found());
    // writing through a symlink pointing out: its target is a path under the
    // root, where the directories are not, or made inside the root
    EXPECT_TRUE(fs.write_file(sgcl::string("/abs"), slice<const byte>("x")).error().is_not_found());
    ASSERT_TRUE(fs.mkdir(sgcl::string("/outside")));
    ASSERT_TRUE(fs.write_file(sgcl::string("/rel"), slice<const byte>("inside")));
    EXPECT_EQ(read_file(secret), "do not read");
    EXPECT_EQ(s.disk("outside/secret"), "inside");
    // listing and stat through them see the root's tree
    EXPECT_TRUE(fs.read_dir(sgcl::string("/planted")).error().is_not_found());
    auto up = fs.read_dir(sgcl::string("/up"));
    ASSERT_TRUE(up);
    bool sees_root_parent = false;
    for (auto& e : *up) {
        sees_root_parent |= e.name == "root";   // the served directory's own name: seen only from its parent
    }
    EXPECT_FALSE(sees_root_parent);
    // a hard link of a file outside, a rename out
    EXPECT_FALSE(fs.hard_link(sgcl::string("/planted_file"), sgcl::string("/hl")) && fsys::exists(s.root / "hl") &&
                 read_file((s.root / "hl").string()) == "do not read");
    ASSERT_TRUE(fs.write_file(sgcl::string("/mine"), slice<const byte>("m")));
    ASSERT_TRUE(fs.rename(sgcl::string("/mine"), sgcl::string("/../outside/mine")));
    EXPECT_FALSE(fsys::exists(s.outside / "mine"));
    EXPECT_TRUE(fsys::exists(s.root / "outside" / "mine"));
    // loops end
    ASSERT_TRUE(fs.symlink(sgcl::string("loop2"), sgcl::string("/loop1")));
    ASSERT_TRUE(fs.symlink(sgcl::string("loop1"), sgcl::string("/loop2")));
    auto loop = fs.stat(sgcl::string("/loop1"));
    ASSERT_FALSE(loop);
    EXPECT_TRUE(loop.error().is_not_found()) << err(loop.error());   // ELOOP as SSH_FX_NO_SUCH_FILE, as OpenSSH answers it
    // a FIFO is no file to serve
    ASSERT_EQ(::mkfifo((s.root / "fifo").c_str(), 0600), 0);
    EXPECT_FALSE(fs.open(sgcl::string("/fifo")));
    // the file outside was never touched
    EXPECT_EQ(read_file(secret), "do not read");
}

TEST(SftpLoopback, LargeFilesBothWays) {
    Served s;
    auto& fs = s.fs;
    const size_t size = 48 * 1024 * 1024 + 12345;
    auto base = s.root.parent_path();
    {
        std::ofstream out(base / "big", std::ios::binary);
        std::string block(1 << 20, 0);
        for (size_t at = 0; at < size; at += block.size()) {
            for (size_t i = 0; i < block.size(); ++i) {
                block[i] = char((at + i) * 2654435761u >> 13);
            }
            out.write(block.data(), std::streamsize(std::min(block.size(), size - at)));
        }
    }
    auto up = fs.upload(sgcl::string((base / "big").string()), sgcl::string("/big"));
    ASSERT_TRUE(up) << err(up.error());
    EXPECT_EQ(*up, size);
    EXPECT_EQ(fsys::file_size(s.root / "big"), size);
    auto down = fs.download(sgcl::string("/big"), sgcl::string((base / "back").string()));
    ASSERT_TRUE(down) << err(down.error());
    EXPECT_EQ(*down, size);
    EXPECT_TRUE(read_file((base / "big").string()) == read_file((base / "back").string()));
    auto whole = fs.read_file(sgcl::string("/big"));
    ASSERT_TRUE(whole);
    EXPECT_EQ(whole->size(), size);
    // empty files
    ASSERT_TRUE(fs.write_file(sgcl::string("/empty"), slice<const byte>()));
    EXPECT_EQ(fs.read_file(sgcl::string("/empty"))->size(), 0u);
    std::ofstream(base / "empty_local");
    EXPECT_EQ(*fs.upload(sgcl::string((base / "empty_local").string()), sgcl::string("/e2")), 0u);
    EXPECT_EQ(*fs.download(sgcl::string("/e2"), sgcl::string((base / "e3").string())), 0u);
    // errors of a transfer
    EXPECT_TRUE(fs.upload(sgcl::string((base / "none").string()), sgcl::string("/x")).error().is_not_found());
    EXPECT_TRUE(fs.download(sgcl::string("/none"), sgcl::string((base / "y").string())).error().is_not_found());
    EXPECT_TRUE(fs.upload(sgcl::string((base / "big").string()), sgcl::string("/no/dir/x")).error().is_not_found());
}

TEST(SftpLoopback, RequestsFromManyTasksAndThreads) {
    Served s;
    net::sftp::client fs = s.fs;
    for (int i = 0; i < 16; ++i) {
        std::ofstream(s.root / ("f" + std::to_string(i))) << std::string(size_t(1000 + i * 977), char('a' + i));
    }
    std::atomic<int> good{0};
    vector<thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.push_back(thread([&, t] {
            for (int i = 0; i < 16; ++i) {
                int k = (t + i) % 16;
                auto b = fs.read_text(sgcl::string("/f" + std::to_string(k)));
                if (b && text(*b) == std::string(size_t(1000 + k * 977), char('a' + k))) {
                    ++good;
                }
                (void)fs.stat(sgcl::string("/f0"));
            }
        }));
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(good.load(), 8 * 16);
    // in tasks
    auto task = [](net::sftp::client c, int k) -> async::task<bool> {
        auto w = co_await c.async_write_file(sgcl::string("/t" + std::to_string(k)), slice<const byte>(std::string_view("task")));
        auto r = co_await c.async_read_text(sgcl::string("/t" + std::to_string(k)));
        co_return w && r && *r == "task";
    };
    vector<async::task<bool>> tasks;
    for (int k = 0; k < 32; ++k) {
        tasks.push_back(async::spawn(task(fs, k)));
    }
    int ok = 0;
    for (auto& t : tasks) {
        ok += t.wait() ? 1 : 0;
    }
    EXPECT_EQ(ok, 32);
}

TEST(SftpLoopback, AReadOnlyServer) {
    net::sftp::server_options o;
    o.read_only = true;
    Served s(o, "sftp_ro");
    std::ofstream(s.root / "f") << "x";
    auto& fs = s.fs;
    EXPECT_EQ(text(*fs.read_text(sgcl::string("/f"))), "x");
    EXPECT_TRUE(fs.write_file(sgcl::string("/g"), slice<const byte>("y")).error().is_permission());
    EXPECT_TRUE(fs.open(sgcl::string("/f"), io::open_flags::write).error().is_permission());
    EXPECT_TRUE(fs.remove(sgcl::string("/f")).error().is_permission());
    EXPECT_TRUE(fs.mkdir(sgcl::string("/d")).error().is_permission());
    EXPECT_TRUE(fs.rename(sgcl::string("/f"), sgcl::string("/g")).error().is_permission());
    EXPECT_TRUE(fs.chmod(sgcl::string("/f"), io::permissions(0777)).error().is_permission());
    EXPECT_TRUE(fs.symlink(sgcl::string("f"), sgcl::string("/l")).error().is_permission());
    EXPECT_EQ(read_file((s.root / "f").string()), "x");
}

TEST(SftpLoopback, AClosedSessionAndBoundaries) {
    Served s;
    net::sftp::client fs = s.fs;
    EXPECT_FALSE(net::sftp::client());
    EXPECT_FALSE(net::sftp::file());
    auto f = *fs.create(sgcl::string("/f"));
    EXPECT_EQ(*f.write(slice<const byte>()), 0u);   // nothing
    vector<byte> none;
    EXPECT_EQ(*f.read(none.as_slice()), 0u);
    EXPECT_EQ(f.path(), "/f");
    ASSERT_TRUE(fs.close());
    EXPECT_TRUE(fs.is_closed());
    EXPECT_EQ(fs.stat(sgcl::string("/f")).error().code(), io::errc::closed);
    EXPECT_EQ(f.write(sgcl::string("x")).error().code(), io::errc::closed);
    ASSERT_TRUE(fs.close());   // twice: nothing
    s.fs = net::sftp::client();
    // a session over an SSH connection there is, which stays open after
    auto c = *net::ssh::client::connect(sgcl::string("127.0.0.1:" + std::to_string(s.port)), [] {
        auto o = client_options();
        o.keys = {};
        return o;
    }());
    auto fs2 = net::sftp::client::connect(c);
    ASSERT_TRUE(fs2);
    EXPECT_TRUE(fs2->stat(sgcl::string("/f")));
    ASSERT_TRUE(fs2->close());
    EXPECT_FALSE(c.is_closed());
    EXPECT_EQ(text(c.run(sgcl::string("x"))->out), "");   // the connection still serves
    // a server whose sessions are no SFTP: its silence ends the wait
    net::ssh::server silent;
    silent.host_keys = {key("ed25519")};
    silent.no_client_auth = true;
    silent.handle([](net::ssh::server_session x) {});   // ends at once
    LocalServer plain(silent);
    auto co = client_options();
    co.keys = {};
    auto c2 = *net::ssh::client::connect(plain.address(), co);
    EXPECT_FALSE(net::sftp::client::connect(c2));
    // the root not there
    net::ssh::server bad;
    bad.host_keys = {key("ed25519")};
    bad.no_client_auth = true;
    std::atomic<bool> refused{false};
    bad.handle([&refused](net::ssh::server_session x) {
        auto r = net::sftp::serve(x, sgcl::string("/no/such/root"));
        refused = !r && r.error().is_not_found();
    });
    LocalServer b(bad);
    auto o = client_options();
    o.keys = {};
    EXPECT_FALSE(net::sftp::client::connect(b.address(), o));
    EXPECT_TRUE(refused.load());
}
