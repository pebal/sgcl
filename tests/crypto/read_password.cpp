//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::read_password: a password typed on the process's own terminal,
// read with the echo off into secret bytes. The terminal is this program's
// /dev/tty, so the reading side runs in this program started again as a
// child on a pseudo-terminal (io::pty), which it then has for its controlling
// terminal; a child in a session without one gets ENXIO.
#include "tests/types.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace {
    namespace io = sgcl::io;
    namespace crypto = sgcl::crypto;
    using namespace std::chrono_literals;
    using sgcl::string;

    const char* const Filter = "--gtest_filter=CryptoReadPassword_Tests.FromTheTerminal";   // one test: one password read

    // The child's side: the password read and printed back as hex with its
    // length, so that a byte the terminal mangled shows
    bool child() {
        if (!std::getenv("SGCL_CRYPTO_PASSWORD_CHILD")) {
            return false;
        }
        auto pw = crypto::read_password("Password: ");
        std::string out;
        if (pw) {
            out = "got:" + std::to_string(pw->size()) + ":";
            for (auto b : pw->as_slice()) {
                out += char(b);
            }
        } else {
            out = "error:" + std::string(pw.error().message().view());
        }
        out += "\n";
        (void)io::stdout.write(string(out));
        return true;
    }

    io::command self() {
        io::command c(io::executable().value(), Filter);
        auto env = io::environ();
        env.push_back({"SGCL_CRYPTO_PASSWORD_CHILD", "1"});
        c.env = env;
        return c;
    }

    // The child on a terminal: the prompt awaited, `typed` written, the
    // screen to the end
    std::string typed_into(const char* typed) {
        io::pty p = io::open_pty().value();
        io::command c = self();
        EXPECT_TRUE(p.start(c));
        std::string seen;
        auto deadline = std::chrono::steady_clock::now() + 20s;
        while (seen.find("Password: ") == std::string::npos && std::chrono::steady_clock::now() < deadline) {
            sgcl::byte b[256];
            auto n = p.read(sgcl::slice<sgcl::byte>(b, sizeof b));
            if (!n || *n == 0) {
                break;
            }
            seen.append(reinterpret_cast<const char*>(b), *n);
        }
        std::this_thread::sleep_for(100ms);   // the echo off after the prompt
        EXPECT_TRUE(p.write(typed));
        seen += std::string(io::read_all_text(p).value().view());
        (void)c.wait();
        return seen;
    }
}

TEST(CryptoReadPassword_Tests, FromTheTerminal) {
    if (child()) {
        return;
    }
    std::string screen = typed_into("hunter2\n");
    EXPECT_NE(screen.find("Password: \r\ngot:7:hunter2"), std::string::npos) << screen;   // the new line after the hidden input
    EXPECT_EQ(screen.find("hunter2"), screen.find("got:7:hunter2") + 6) << screen;       // never echoed
}

TEST(CryptoReadPassword_Tests, EditingAndTheEnd) {
    if (child()) {
        return;
    }
    // a backspace and a DEL take the last byte back; "\r" ends the line too
    EXPECT_NE(typed_into("abx\bq\x7f" "c\r").find("got:3:abc"), std::string::npos);
    // a password longer than the bytes the secret holds in itself (64)
    std::string long_one(150, 'k');
    EXPECT_NE(typed_into((long_one + "\n").c_str()).find("got:150:" + long_one), std::string::npos);
    // an empty line is an empty password; ^D before anything, the input's end
    EXPECT_NE(typed_into("\n").find("got:0:"), std::string::npos);
    EXPECT_NE(typed_into("\x04").find("error:read_password /dev/tty: unexpected end of stream"), std::string::npos);
}

TEST(CryptoReadPassword_Tests, WithoutATerminal) {
    if (child()) {
        return;
    }
    // a session of its own and no controlling terminal: a pty's start with
    // none of the three streams the terminal
    io::pty p = io::open_pty().value();
    io::command c = self();
    io::buffer in;
    io::buffer out;
    c.in = in;
    c.out = out;
    c.err = io::discard;
    ASSERT_TRUE(p.start(c));
    ASSERT_TRUE(c.wait());
    EXPECT_NE(out.text().find("error:read_password /dev/tty: Device not configured"), string::npos) << out.text().view();
}
