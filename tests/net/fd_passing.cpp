//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::connection::send_descriptors and receive_descriptors: descriptors
// passed with bytes over a unix-domain connection (SCM_RIGHTS) — a file, a
// pipe's end, a socket, several in one message, from a thread and from a
// task, the refusals; and with Python's socket.send_fds and recv_fds in
// another process as the other side.
#include "tests/types.h"
#include "sgcl/net/net.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
    namespace io = sgcl::io;
    namespace net = sgcl::net;
    using sgcl::string;
    using sgcl::vector;
    using namespace std::chrono_literals;
    using expected_size = sgcl::expected<size_t, io::error>;

    // The listener and the two ends of a connection: on the test's stack,
    // never in the fixture (gtest's new memory, where the collector does not
    // look: a handle there is lost at the first cycle)
    struct Ends {
        net::listener l;
        net::connection a;   // the client's end
        net::connection b;   // the server's

        explicit Ends(const std::string& dir) {
            l = net::unix_domain::listen(string(dir + "/s")).value();
            a = net::unix_domain::connect(string(dir + "/s")).value();
            b = l.accept().value();
        }

        Ends(const Ends&) = delete;

        ~Ends() {
            (void)a.close();
            (void)b.close();
            (void)l.close();
        }
    };

    struct NetFdPassing_Tests : testing::Test {
        std::string _dir;

        void SetUp() override {
            _dir = io::make_temp_dir({}, "fdp-").value().str();   // short: a socket's path is at most 103 bytes
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return string(_dir + "/" + name);
        }
    };

    std::string text_of(const io::file& f) {
        auto t = io::read_all_text(f);
        return t ? std::string(t->view()) : std::string("error");
    }
}

TEST_F(NetFdPassing_Tests, AFile) {
    Ends e(_dir);
    net::listener& l = e.l;
    net::connection& a = e.a;
    net::connection& b = e.b;
    ASSERT_TRUE(io::write_file(at("data.txt"), "passed along"));
    io::file f = io::open(at("data.txt")).value();
    auto sent = a.send_descriptors(string("one file"), std::array{f.fd()});
    ASSERT_TRUE(sent) << sent.error().message();
    EXPECT_EQ(*sent, 8u);
    vector<sgcl::byte> buf(64);
    auto got = b.receive_descriptors(buf);
    ASSERT_TRUE(got) << got.error().message();
    EXPECT_EQ(got->size, 8u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), got->size), "one file");
    ASSERT_EQ(got->files.size(), 1u);
    io::file mine = got->files[0];
    EXPECT_NE(mine.fd(), f.fd());                              // a descriptor of its own
    EXPECT_TRUE(::fcntl(mine.fd(), F_GETFD) & FD_CLOEXEC);    // closed in children
    EXPECT_EQ(text_of(mine), "passed along");
    EXPECT_EQ(text_of(f), "");                                 // one open file: the position shared
    ASSERT_TRUE(mine.close());
    EXPECT_FALSE(f.is_closed());                               // the sender's own stays
}

TEST_F(NetFdPassing_Tests, APipeAndSeveralAtOnce) {
    Ends e(_dir);
    net::listener& l = e.l;
    net::connection& a = e.a;
    net::connection& b = e.b;
    auto p = io::pipe().value();
    io::file f1 = io::create(at("x")).value();
    io::file f2 = io::create(at("y")).value();
    ASSERT_TRUE(a.send_descriptors(string("three"), std::array{p.write.fd(), f1.fd(), f2.fd()}));
    vector<sgcl::byte> buf(16);
    auto got = b.receive_descriptors(buf);
    ASSERT_TRUE(got);
    ASSERT_EQ(got->files.size(), 3u);
    ASSERT_TRUE(got->files[0].write("through the passed end"));
    ASSERT_TRUE(got->files[0].close());
    ASSERT_TRUE(p.write.close());
    EXPECT_EQ(text_of(p.read), "through the passed end");
    ASSERT_TRUE(got->files[2].write("to y"));
    EXPECT_EQ(io::read_text(at("y")).value(), "to y");
}

TEST_F(NetFdPassing_Tests, ASocket) {
    Ends e(_dir);
    net::listener& l = e.l;
    net::connection& a = e.a;
    net::connection& b = e.b;
    // the server's end of another connection, passed: written through by the receiver
    net::connection c = net::unix_domain::connect(string(_dir + "/s")).value();
    net::connection d = l.accept().value();
    ASSERT_GE(d.fd(), 0);
    EXPECT_EQ(net::connection::in_memory().first.fd(), -1);   // no socket of its own
    ASSERT_TRUE(a.send_descriptors(string("s"), std::array{d.fd()}));
    vector<sgcl::byte> buf(4);
    auto got = b.receive_descriptors(buf);
    ASSERT_TRUE(got);
    ASSERT_EQ(got->files.size(), 1u);
    ASSERT_TRUE(got->files[0].write("over a passed socket"));
    vector<sgcl::byte> in(64);
    auto n = c.read(in);
    ASSERT_TRUE(n);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(in.data()), *n), "over a passed socket");
    (void)c.close();
    (void)d.close();
}

TEST_F(NetFdPassing_Tests, Refusals) {
    Ends e(_dir);
    net::listener& l = e.l;
    net::connection& a = e.a;
    net::connection& b = e.b;
    io::file f = io::create(at("r")).value();
    auto empty = a.send_descriptors(string(), std::array{f.fd()});
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code(), std::errc::invalid_argument);
    EXPECT_EQ(empty.error().op(), "send_descriptors");
    auto none = a.send_descriptors(string("x"), sgcl::slice<const int>());
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), std::errc::invalid_argument);
    // more than the reader takes: what came closed, EMSGSIZE
    io::file g = io::create(at("s2")).value();
    ASSERT_TRUE(a.send_descriptors(string("two"), std::array{f.fd(), g.fd()}));
    vector<sgcl::byte> buf(8);
    int stdin_flags = ::fcntl(0, F_GETFD);
    auto cut = b.receive_descriptors(buf, 1);
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), std::errc::message_size);
    // only what came is closed: macOS keeps the cut header's full length, and
    // the ints past the control block were taken for descriptor 0 (stdin),
    // closed, and every later program started lost it
    EXPECT_EQ(::fcntl(0, F_GETFD), stdin_flags);
    // bytes alone come with no descriptor
    ASSERT_TRUE(a.write("plain"));
    auto plain = b.receive_descriptors(buf);
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->size, 5u);
    EXPECT_TRUE(plain->files.empty());
    // read_line's buffer holding bytes: refused, not skipped
    ASSERT_TRUE(a.write("line\nrest"));
    auto line = b.read_line();
    ASSERT_TRUE(line && *line);
    auto busy = b.receive_descriptors(buf);
    ASSERT_FALSE(busy);
    EXPECT_EQ(busy.error().code(), std::errc::device_or_resource_busy);
    // a TCP connection carries none
    auto tl = net::tcp::listen("127.0.0.1:0").value();
    auto t = net::tcp::connect(tl.local_endpoint().to_string()).value();
    auto tcp = t.send_descriptors(string("x"), std::array{f.fd()});
    ASSERT_FALSE(tcp);
    EXPECT_EQ(tcp.error().code(), std::errc::operation_not_supported);
    // a closed connection
    ASSERT_TRUE(a.close());
    auto closed = a.send_descriptors(string("x"), std::array{f.fd()});
    ASSERT_FALSE(closed);
    EXPECT_TRUE(closed.error().is_closed());
    (void)t.close();
    (void)tl.close();
}

namespace {
    sgcl::async::task<std::string> pass_in_tasks(net::connection a, net::connection b, io::file f) {
        auto sent = co_await a.async_send_descriptors(string("async"), std::array{f.fd()});
        if (!sent) {
            co_return "send: " + std::string(sent.error().message().view());
        }
        vector<sgcl::byte> buf(16);
        auto got = co_await b.async_receive_descriptors(buf);
        if (!got || got->files.size() != 1) {
            co_return "receive";
        }
        auto t = co_await got->files[0].async_read_all_text();
        co_return t ? std::string(t->view()) : "read";
    }
}

TEST_F(NetFdPassing_Tests, FromATask) {
    Ends e(_dir);
    net::listener& l = e.l;
    net::connection& a = e.a;
    net::connection& b = e.b;
    ASSERT_TRUE(io::write_file(at("t.txt"), "in a task"));
    io::file f = io::open(at("t.txt")).value();
    EXPECT_EQ(sgcl::async::spawn(pass_in_tasks(a, b, f)).wait(), "in a task");
    // a receive waits for its message on the reactor
    auto waiting = sgcl::async::spawn(pass_in_tasks(a, b, io::open(at("t.txt")).value()));
    EXPECT_EQ(waiting.wait(), "in a task");
}

// 253 descriptors in one message (SCM_MAX_FD), 254 refused before a call;
// the pair in memory carries none either way
TEST_F(NetFdPassing_Tests, Limits) {
    Ends e(_dir);
    net::connection& a = e.a;
    net::connection& b = e.b;
    io::file f = io::create(at("many")).value();
    std::array<int, 254> fds;
    fds.fill(f.fd());
    auto over = a.send_descriptors(string("x"), fds);
    ASSERT_FALSE(over);
    EXPECT_EQ(over.error().code(), std::errc::invalid_argument);
    auto most = a.send_descriptors(string("x"), sgcl::slice<const int>(fds.data(), 253));
    ASSERT_TRUE(most) << most.error().message();
    vector<sgcl::byte> buf(4);
    auto got = b.receive_descriptors(buf, 253);
    ASSERT_TRUE(got) << got.error().message();
    EXPECT_EQ(got->files.size(), 253u);
    for (auto& g : got->files) {
        ASSERT_TRUE(g.close());
    }
    auto pair = net::connection::in_memory();
    auto s = pair.first.send_descriptors(string("x"), std::array{f.fd()});
    ASSERT_FALSE(s);
    EXPECT_EQ(s.error().code(), std::errc::operation_not_supported);
    auto r = pair.second.receive_descriptors(buf);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::operation_not_supported);
}

namespace {
    // The task form copies the descriptors before it starts: the array it
    // was given is gone when the task runs
    sgcl::async::task<expected_size> send_later(net::connection a, int fd) {
        auto t = [&] {
            int fds[] = {fd};
            return a.async_send_descriptors(string("later"), fds);
        }();
        int clobber[] = {-1};
        (void)clobber;
        co_return co_await std::move(t);
    }
}

TEST_F(NetFdPassing_Tests, TaskCopiesTheDescriptors) {
    Ends e(_dir);
    net::connection& a = e.a;
    net::connection& b = e.b;
    ASSERT_TRUE(io::write_file(at("l.txt"), "copied"));
    io::file f = io::open(at("l.txt")).value();
    auto sent = sgcl::async::spawn(send_later(a, f.fd())).wait();
    ASSERT_TRUE(sent) << sent.error().message();
    EXPECT_EQ(*sent, 5u);
    vector<sgcl::byte> buf(8);
    auto got = sgcl::async::spawn(b.async_receive_descriptors(buf)).wait();
    ASSERT_TRUE(got) << got.error().message();
    ASSERT_EQ(got->files.size(), 1u);
    EXPECT_EQ(text_of(got->files[0]), "copied");
}

// Python's socket.send_fds in another process, and its recv_fds of ours
TEST_F(NetFdPassing_Tests, WithPython) {
    Ends e(_dir);
    net::listener& l = e.l;
    net::connection& a = e.a;
    net::connection& b = e.b;
    if (std::system("python3 -c 'import socket; socket.send_fds' > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no python3 with socket.send_fds (3.9)";
    }
    io::write_file(at("py.txt"), "from python").value();
    std::string script =
        "import socket, os\n"
        "s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)\n"
        "s.connect('" + _dir + "/s')\n"
        "f = os.open('" + at("py.txt").str() + "', os.O_RDONLY)\n"
        "socket.send_fds(s, [b'py'], [f])\n"
        "msg, fds, flags, addr = socket.recv_fds(s, 16, 4)\n"
        "os.write(1, msg + b':' + os.read(fds[0], 100))\n";
    io::write_file(at("peer.py"), string(script)).value();
    io::command peer("python3", at("peer.py"));
    io::buffer out;
    peer.out = out;
    ASSERT_TRUE(peer.start());
    net::connection p = l.accept().value();
    vector<sgcl::byte> buf(16);
    auto got = p.receive_descriptors(buf);
    ASSERT_TRUE(got) << got.error().message();
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), got->size), "py");
    ASSERT_EQ(got->files.size(), 1u);
    EXPECT_EQ(text_of(got->files[0]), "from python");
    io::write_file(at("ours.txt"), "from sgcl").value();
    io::file mine = io::open(at("ours.txt")).value();
    ASSERT_TRUE(p.send_descriptors(string("sgcl"), std::array{mine.fd()}));
    ASSERT_TRUE(peer.wait());
    EXPECT_EQ(out.text(), "sgcl:from sgcl");
    (void)p.close();
}
