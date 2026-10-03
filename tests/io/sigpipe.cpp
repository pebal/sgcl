//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io: a write to a pipe whose read end is closed is the error EPIPE, never
// SIGPIPE, which ends the process (net's sockets the same). The cases run
// in a process of their own (gtest's death test): before, the signal
// ended it. A file of its own: the death test names the C stream stderr,
// which `using namespace sgcl::io` of the other files makes ambiguous
#include "tests/types.h"

#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
    namespace io = sgcl::io;

    bool broken(const expected<size_t, io::error>& w) {
        return !w && w.error().code() == std::errc::broken_pipe;
    }

    // The case's exit code: 0 when every write was EPIPE, else which was not
    int writes_without_a_reader() {
        auto p = io::pipe();
        if (!p) {
            return 2;
        }
        (void)p->read.close();
        if (!broken(p->write.write("x"))) {
            return 3;
        }
        if (!broken(sgcl::async::spawn(p->write.async_write(std::string("x"))).wait())) {
            return 4;
        }
        auto dir = io::make_temp_dir({}, "sgcl-sigpipe-*");
        if (!dir) {
            return 5;
        }
        sgcl::string fifo = io::path::join(*dir, "fifo");
        if (::mkfifo(fifo.c_str(), 0600) != 0) {
            return 6;
        }
        int keep = ::open(fifo.c_str(), O_RDONLY | O_NONBLOCK);   // a reader, so that the open for writing does not wait
        auto f = io::open(fifo, io::open_flags::write);
        ::close(keep);
        bool fifo_broken = f && broken(f->write("x"));
        (void)io::remove_all(*dir);
        if (!fifo_broken) {
            return 7;
        }
        int fds[2];
        if (::pipe(fds) != 0) {
            return 8;
        }
        ::close(fds[0]);
        if (!broken(io::from_fd(fds[1], "given").write("x"))) {
            return 9;
        }
        return 0;
    }
}

TEST(IoSigpipe_Tests, AWriteToAPipeWithoutAReaderIsEpipe) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(std::_Exit(writes_without_a_reader()), testing::ExitedWithCode(0), "");
}
