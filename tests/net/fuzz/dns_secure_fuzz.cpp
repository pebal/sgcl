//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The answers of DNS over TLS and DNS over HTTPS (sgcl/net/detail/dns_tls.h,
// sgcl/net/http/detail/doh.h) on any bytes, without an oracle. The first
// byte chooses. Even: DoT — the rest is what the server's side of a
// connection in memory sends, read by the transport's reader (dot_read)
// while four queries wait, their ids the first four of the input's frames
// (or fixed); odd: DoH — the rest is a response: a status, a content
// type, a body. What must hold:
//   - DoT: the stream is read as frames of a length and a message, a frame
//     shorter than a header ending it; each waiting query gets the frame of
//     its id or is told the connection failed, and never a frame of another
//     id; the reader ends with the connection, and fails the rest;
//   - an answer handed over reads as the answer to its question, ok only
//     with records;
//   - DoH: a status outside 2xx, a type not application/dns-message, a body
//     past 65535 bytes is no answer; any other body reads as the answer to
//     its question with id 0 or is the server's fault.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/dns_secure_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/http.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    namespace nd = sgcl::net::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // The question of a message when it has one, else www.example.com A
    void question_of(const uint8_t* p, size_t n, nd::DnsName& name, uint16_t& type) {
        nd::DnsReader r(p, n);
        nd::DnsHeader h;
        uint16_t klass = 0;
        if (!r.header(h) || h.questions < 1 || !r.question(name, type, klass)) {
            nd::dns_name_from_text("www.example.com.", name);
            type = nd::dns_type::a;
        }
    }

    void answer_sound(const nd::DnsAnswer& a) {
        if (a.status == nd::DnsStatus::ok) {
            check(!a.records.empty());
        } else {
            check(a.records.empty() || a.status == nd::DnsStatus::ok);
        }
    }

    async::task<> feed(net::connection c, vector<byte> bytes) {
        if (!bytes.empty()) {
            (void)co_await c.async_write(bytes.as_slice());
        }
        (void)c.close();
    }

    void dot(const uint8_t* data, size_t size) {
        auto [ours, theirs] = net::connection::in_memory();
        tracked_ptr<nd::DotConn> conn = make_tracked<nd::DotConn>();
        conn->c = ours;
        // the ids of the first four frames, waited for
        vector<tracked_ptr<nd::DotWait>> waits;
        std::vector<uint16_t> ids;
        size_t p = 0;
        while (ids.size() < 4 && p + 4 <= size) {
            size_t len = size_t(data[p]) << 8 | data[p + 1];
            ids.push_back(uint16_t(data[p + 2] << 8 | data[p + 3]));
            p += 2 + len;
        }
        for (uint16_t fixed : {uint16_t(0), uint16_t(1), uint16_t(0xFFFF), uint16_t(0x1234)}) {
            if (ids.size() >= 4) {
                break;
            }
            ids.push_back(fixed);
        }
        for (uint16_t id : ids) {
            tracked_ptr<nd::DotWait> w = make_tracked<nd::DotWait>();
            bool dup = false;
            {
                std::lock_guard lock(conn->m);
                dup = conn->waiting.find(id) != conn->waiting.end();
                if (!dup) {
                    conn->waiting.insert_or_assign(id, w);
                }
            }
            if (!dup) {
                waits.push_back(w);
            }
        }
        vector<byte> bytes(size);
        nd::copy_bytes(bytes.data(), data, size);
        auto writer = async::spawn(feed(theirs, std::move(bytes)));
        nd::dot_read(conn).wait();
        (void)theirs.close();
        writer.wait();
        check(!conn->alive());
        for (size_t i = 0; i < waits.size(); ++i) {
            auto got = waits[i]->done.try_receive();
            check(got.has_value());
            if (*got) {
                auto& a = waits[i]->answer;
                check(a.size() >= nd::DnsHeaderSize);
                uint16_t id = uint16_t(uint16_t(a[0]) << 8 | uint16_t(a[1]));
                bool ok = false;
                for (uint16_t x : ids) {
                    ok = ok || x == id;
                }
                check(ok);
                nd::DnsName q;
                uint16_t t = 0;
                question_of(reinterpret_cast<const uint8_t*>(a.data()), a.size(), q, t);
                nd::DnsAnswer out;
                nd::dns_read_answer(reinterpret_cast<const uint8_t*>(a.data()), a.size(), id, q, t, true, out);
                answer_sound(out);
            }
        }
    }

    void doh(const uint8_t* data, size_t size) {
        if (size < 2) {
            return;
        }
        int status = 100 + data[0] * 2;
        size_t tlen = data[1] % 40;
        if (size - 2 < tlen) {
            return;
        }
        string type(std::string_view(reinterpret_cast<const char*>(data + 2), tlen));
        const uint8_t* body = data + 2 + tlen;
        size_t blen = size - 2 - tlen;
        bool message = nd::doh_is_message(type);
        tracked_ptr<nd::DohWait> w = make_tracked<nd::DohWait>();
        w->misbehaving = status < 200 || status > 299 || !message;
        w->answer = vector<byte>(blen);
        nd::copy_bytes(w->answer.data(), body, blen);
        w->size = blen;
        nd::DnsName q;
        uint16_t t = 0;
        question_of(body, blen, q, t);
        nd::DnsAnswer a = nd::doh_answer(*w, q, t);
        if (w->misbehaving || blen > nd::DohMaxAnswer) {
            check(a.status == nd::DnsStatus::misbehaving);
        }
        check(a.status != nd::DnsStatus::foreign && a.status != nd::DnsStatus::truncated);
        answer_sound(a);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    if (data[0] & 1) {
        doh(data + 1, size - 1);
    } else {
        dot(data + 1, size - 1);
    }
    return 0;
}
