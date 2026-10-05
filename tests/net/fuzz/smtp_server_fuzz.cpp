//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::smtp's server machine (detail::ServerMachine) on any bytes from a
// client: the commands and their parameters, DATA, BDAT, AUTH, STARTTLS.
// The first byte picks the settings (AUTH offered, STARTTLS offered, small
// limits), the second the size of the pieces the input is fed in. What
// must hold:
//   - every reply written is lines of a code of 200 to 599, a space or a
//     dash, and text, each ended by CRLF, the dash only before a line of
//     the same code;
//   - the input fed whole and fed in pieces gives the same replies and the
//     same messages (the machine keeps nothing of how the bytes came);
//   - a message's data never passes the limit; STARTTLS leaves nothing of
//     the clear text buffered.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/smtp_server_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/smtp.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace smtp = sgcl::net::smtp;
    namespace sd = sgcl::net::smtp::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void replies_well_formed(std::string_view out) {
        int prev = 0;
        while (!out.empty()) {
            size_t nl = out.find("\r\n");
            check(nl != std::string_view::npos);
            std::string_view line = out.substr(0, nl);
            check(line.size() >= 3 && line[0] >= '2' && line[0] <= '5' && line[1] >= '0' && line[1] <= '9' && line[2] >= '0' && line[2] <= '9');
            int code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
            if (prev) {
                check(code == prev);
            }
            check(line.size() == 3 || line[3] == ' ' || line[3] == '-');
            prev = line.size() > 3 && line[3] == '-' ? code : 0;
            for (char c : line) {
                check(c != '\r' && c != '\n');
            }
            out.remove_prefix(nl + 2);
        }
        check(prev == 0);
    }

    std::string run(std::string_view in, uint8_t mode, size_t piece) {
        sd::ServerMachine m;
        m.cfg.hostname = string("mx.fuzz");
        if (mode & 1) {
            m.cfg.auth_offered = true;
            m.cfg.allow_insecure_auth = true;
            m.cfg.auth = [](const string& u, const string& p) { return u == "alice" && p == "secret"; };
        }
        if (mode & 2) {
            m.cfg.tls_offered = true;
        }
        if (mode & 4) {
            m.cfg.max_message_bytes = 64;
            m.cfg.max_recipients = 2;
            m.cfg.max_line_bytes = 80;
            m.cfg.max_errors = 4;
            m.cfg.max_junk_commands = 6;
        }
        if (mode & 8) {
            m.cfg.on_recipient = [](const smtp::envelope&, const string& to) {
                return to.view().starts_with("bad") ? smtp::reply{550, string("5.1.1"), string("no\nsuch")} : smtp::reply();
            };
        }
        m.greet();
        std::string log;
        bool closed = false;
        for (size_t i = 0; i < in.size() && !closed; i += piece) {
            m.feed(in.substr(i, piece));
            for (;;) {
                auto s = m.step();
                if (s == sd::MachineStep::message) {
                    check(!(mode & 4) || m.data().size() <= 64);
                    log += "[msg " + std::to_string(m.data().size()) + " " + std::string(m.env.from.view()) + "]";
                    m.message_done(m.data().size() % 3 == 0 ? smtp::reply{554, string("5.6.0"), string("no")} : smtp::reply());
                    continue;
                }
                if (s == sd::MachineStep::starttls) {
                    check(m.buffered() == 0);
                    log += "[tls]";
                    m.tls_started();
                    // what came after STARTTLS in this piece is gone; the rest of the input is the encrypted session's
                    continue;
                }
                if (s == sd::MachineStep::close) {
                    closed = true;
                }
                break;
            }
        }
        replies_well_formed(m.out);
        return m.out + "#" + log;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    const uint8_t mode = data[0];
    const size_t piece = size_t(data[1] % 64) + 1;
    std::string_view in(reinterpret_cast<const char*>(data + 2), size - 2);
    auto whole = run(in, mode, in.size() ? in.size() : 1);
    auto pieces = run(in, mode, piece);
    // STARTTLS drops what was buffered after it: in pieces, less may have
    // been there; the two agree only without it
    if (whole.find("[tls]") == std::string::npos && pieces.find("[tls]") == std::string::npos) {
        check(whole == pieces);
    }
    return 0;
}
