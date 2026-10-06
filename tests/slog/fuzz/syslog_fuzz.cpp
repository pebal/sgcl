//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// slog::syslog's structured data and slog::journald's fields over any bytes:
// a message, keys and values read from the input are logged, and what the
// handlers wrote is read back by a reader of the formats written here.
// RFC 5424: the header's fields where the ABNF puts them, the SD-ELEMENT
// read param by param (an SD-NAME of printable ASCII but = space ] ", a
// value whose " \ ] are escaped), every value back as the value's text,
// the message after it as given. The journal: the datagram read field by
// field (KEY=value, or the binary form for a value with a newline), every
// name upper-case letters, digits and _, not starting with _ or a digit,
// the message and every value back byte for byte. And octet counting: the
// length before a message is its length.
//
// The input: the message, then attributes, a key and a value each, every
// piece after a byte of its length.
//
// Built with libFuzzer (tests/fuzz/run.sh tests/slog/fuzz/syslog_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/io.h"
#include "sgcl/slog.h"
#include "sgcl/slog/syslog.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <source_location>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

namespace {
    using namespace sgcl;

    void check(bool ok, std::source_location at = std::source_location::current()) {
        if (!ok) {
            std::fprintf(stderr, "syslog_fuzz: check failed at line %u\n", (unsigned)at.line());
            __builtin_trap();
        }
    }

    struct Reader {
        const uint8_t* p;
        const uint8_t* end;

        bool piece(std::string& out) {
            if (p >= end) {
                return false;
            }
            size_t n = *p++;
            if (size_t(end - p) < n) {
                n = size_t(end - p);
            }
            out.assign(reinterpret_cast<const char*>(p), n);
            p += n;
            return true;
        }
    };

    // The SD-PARAMs of an SD-ELEMENT that starts at s[at] == '[': the
    // names and the unescaped values, and where it ends
    size_t read_sd(const std::string& s, size_t at, std::vector<std::pair<std::string, std::string>>& out) {
        check(s[at] == '[');
        ++at;
        while (at < s.size() && s[at] != ' ' && s[at] != ']') {
            ++at;   // the SD-ID
        }
        while (s[at] == ' ') {
            ++at;
            std::string name;
            while (s[at] != '=') {
                char c = s[at++];
                check(c > 32 && c < 127 && c != ']' && c != '"');
                name += c;
            }
            check(!name.empty() && name.size() <= 32);
            ++at;
            check(s[at++] == '"');
            std::string value;
            for (;;) {
                check(at < s.size());
                char c = s[at++];
                if (c == '\\') {
                    check(at < s.size());
                    char e = s[at];
                    if (e == '"' || e == '\\' || e == ']') {
                        value += e;
                        ++at;
                    } else {
                        value += c;   // a backslash before anything else is itself (RFC 5424 6.3.3)
                    }
                } else if (c == '"') {
                    break;
                } else {
                    check(c != ']');
                    value += c;
                }
            }
            out.emplace_back(name, value);
        }
        check(s[at] == ']');
        return at + 1;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Reader in{data, data + size};
    std::string message;
    if (!in.piece(message)) {
        return 0;
    }
    std::vector<std::pair<std::string, std::string>> kv;
    std::string k, v;
    while (kv.size() < 8 && in.piece(k) && in.piece(v)) {
        kv.emplace_back(k, v);
    }
    // the record: through a memory handler, so that the attributes are
    // keys and values of any bytes (a key of the call must be a literal)
    slog::memory kept;
    slog::logger keeper(kept);
    auto log_it = [&](auto... a) { keeper.info(slog::message(string(message.c_str(), message.size())), a...); };
    std::vector<string> keys, values;   // the strings outlive the call
    (void)keys;
    (void)values;
    switch (kv.size()) {
    case 0: log_it(); break;
    case 1: log_it("k0", string(kv[0].second.data(), kv[0].second.size())); break;
    default: log_it("k0", string(kv[0].second.data(), kv[0].second.size()), "k1", string(kv[1].second.data(), kv[1].second.size())); break;
    }
    auto records = kept.records();
    check(records.size() == 1);
    const slog::record& r = records[0];
    // RFC 5424 (format::automatic, the default, over a writer) with structured
    // data, octet counted
    io::buffer out;
    slog::syslog h(out, {.app_name = "fz", .hostname = "h", .octet_counting = true, .structured_data_id = "fz@32473"});
    h.handle(r);
    string written = out.text();
    std::string s(written.data(), written.size());
    size_t sp = s.find(' ');
    check(sp != std::string::npos);
    size_t len = std::stoul(s.substr(0, sp));
    check(len == s.size() - sp - 1);
    std::string m = s.substr(sp + 1);
    check(m[0] == '<');
    size_t version = m.find('>');
    check(version != std::string::npos && m.compare(version, 3, ">1 ") == 0);   // format::automatic over a writer: RFC 5424
    size_t sd_at = m.find(" [fz@32473");
    check(sd_at != std::string::npos);
    std::vector<std::pair<std::string, std::string>> params;
    size_t after = read_sd(m, sd_at + 1, params);
    check(m[after] == ' ');
    check(m.substr(after + 1) == message);
    size_t want = kv.size() >= 2 ? 2 : kv.size();
    check(params.size() == want);
    for (size_t i = 0; i < want; ++i) {
        check(params[i].first == "k" + std::to_string(i));
        check(params[i].second == kv[i].second);
    }
    // the journal, through a socket pair standing in for journald
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) != 0) {
        return 0;
    }
    {
        slog::detail::Buf b;
        slog::detail::journal_message(b, string("fz"), 1, r);
        std::string d(b.data(), b.size());
        std::map<std::string, std::string> fields;
        size_t at = 0;
        while (at < d.size()) {
            size_t nl = d.find('\n', at);
            check(nl != std::string::npos);
            size_t eq = d.find('=', at);
            std::string name, value;
            if (eq != std::string::npos && eq < nl) {
                name = d.substr(at, eq - at);
                value = d.substr(eq + 1, nl - eq - 1);
                at = nl + 1;
            } else {
                name = d.substr(at, nl - at);
                check(d.size() >= nl + 9);
                uint64_t n = 0;
                for (int i = 0; i < 8; ++i) {
                    n |= uint64_t(uint8_t(d[nl + 1 + i])) << (8 * i);
                }
                check(d.size() >= nl + 9 + n + 1);
                value = d.substr(nl + 9, n);
                check(d[nl + 9 + n] == '\n');
                at = nl + 9 + n + 1;
            }
            check(!name.empty() && name.size() <= 64 && name[0] != '_' && !(name[0] >= '0' && name[0] <= '9'));
            for (char c : name) {
                check((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
            }
            fields[name] = value;
        }
        check(fields["MESSAGE"] == message);
        for (size_t i = 0; i < want; ++i) {
            check(fields["K" + std::to_string(i)] == kv[i].second);
        }
    }
    ::close(fds[0]);
    ::close(fds[1]);
    return 0;
}
