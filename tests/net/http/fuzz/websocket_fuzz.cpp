//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::websocket's frames on any bytes. The first byte picks the path
// (its low two bits) and, for the connection, the side, the extension and
// the limit. What must hold:
//   - a frame header read is one the rules allow (no reserved bit or
//     opcode, a control frame whole and of at most 125 bytes, the mask
//     where the side wants it), written back it reads the same, and every
//     prefix shorter than it asks for more;
//   - UTF-8 checked in two pieces cut anywhere says what a decoder of RFC
//     3629 written plainly here says of the whole;
//   - permessage-deflate's decoder on any bytes fails or gives at most
//     its limit, and what the encoder makes of its output decodes to it;
//   - a connection fed the bytes as what its peer sent (in memory) gives
//     messages that are within the limit, text ones UTF-8, and ends in an
//     error, never a hang or a crash.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/websocket_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/http.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void header(const uint8_t* p, size_t n, uint8_t mode) {
        const bool expect_mask = mode & 4, deflate = mode & 8;
        WsFrame f;
        const char* why = "";
        auto st = parse_ws_frame(p, n, expect_mask, deflate, f, why);
        if (st != WsParse::frame) {
            return;
        }
        check(f.header <= n && f.header >= 2 && f.header <= 14);
        check(f.masked == expect_mask);
        check(!f.rsv1 || deflate);
        if (f.control()) {
            check(f.fin && f.length <= 125 && !f.rsv1);
        }
        check(!(f.length >> 63));
        uint8_t out[14];
        size_t h = write_ws_header(out, f.fin, f.rsv1, f.opcode, f.length, f.masked, f.mask);
        WsFrame g;
        check(parse_ws_frame(out, h, expect_mask, deflate, g, why) == WsParse::frame);
        check(g.fin == f.fin && g.rsv1 == f.rsv1 && g.opcode == f.opcode && g.length == f.length && g.mask == f.mask && g.header == h);
        for (size_t cut = 0; cut < h; ++cut) {
            check(parse_ws_frame(out, cut, expect_mask, deflate, g, why) == WsParse::more);
        }
    }

    // RFC 3629 §3-4, plainly: a code point at a time
    bool plain_utf8(const uint8_t* p, size_t n) {
        size_t i = 0;
        while (i < n) {
            uint32_t c = p[i];
            size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
            if (len == 0 || i + len > n) {
                return false;
            }
            uint32_t cp = len == 1 ? c : len == 2 ? c & 0x1F : len == 3 ? c & 0x0F : c & 0x07;
            for (size_t k = 1; k < len; ++k) {
                if ((p[i + k] & 0xC0) != 0x80) {
                    return false;
                }
                cp = (cp << 6) | (p[i + k] & 0x3F);
            }
            static const uint32_t least[] = {0, 0, 0x80, 0x800, 0x10000};
            if (cp < least[len] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
                return false;
            }
            i += len;
        }
        return true;
    }

    void utf8_case(const uint8_t* p, size_t n, uint8_t mode) {
        bool whole = plain_utf8(p, n);
        check(valid_utf8(p, n) == whole);
        size_t cut = n ? (mode * 7u) % (n + 1) : 0;
        Utf8Check c;
        bool ok = c.feed(p, cut) && c.feed(p + cut, n - cut) && c.complete();
        check(ok == whole);
    }

    void deflate_case(const uint8_t* p, size_t n, uint8_t mode) {
        WsDeflate d(false, mode & 4);
        vector<byte> out;
        bool too_large = false;
        const size_t limit = 1 << 16;
        if (d.decompress(p, n, out, limit, too_large)) {
            check(out.size() <= limit);
        } else {
            check(!too_large || out.size() <= limit);
        }
        // the encoder's own: decoded to what it was
        WsDeflate a(mode & 8, false), b(false, mode & 8);
        for (int round = 0; round < 2; ++round) {
            std::vector<uint8_t> c;
            a.compress(p, n, c);
            vector<byte> back;
            check(b.decompress(c.data(), c.size(), back, n + 1, too_large));
            check(back.size() == n && (n == 0 || std::equal(p, p + n, reinterpret_cast<const uint8_t*>(back.data()))));
        }
    }

    async::task<> drain(net::connection c) {
        vector<byte> buf(4096);
        for (;;) {
            auto r = co_await c.async_read(buf.as_slice());
            if (!r || *r == 0) {
                co_return;
            }
        }
    }

    async::task<> connection(std::string input, uint8_t mode) {
        auto [ours, theirs] = net::connection::in_memory();
        (void)theirs.close_write();   // after the input, the end of the stream
        async::go(drain(theirs));
        net::http::websocket::options o;
        o.max_message_bytes = mode & 16 ? 100 : 1 << 16;
        optional<WsDeflateParams> d;
        if (mode & 8) {
            d = WsDeflateParams();
        }
        auto ws = WebSocketAccess::make(ours, (mode & 4) != 0, input, o, string(), d);
        for (int i = 0; i < 1000; ++i) {
            auto m = co_await ws.async_receive();
            if (!m) {
                break;
            }
            check(m->data.size() <= o.max_message_bytes);
            if (!m->binary) {
                check(valid_utf8(reinterpret_cast<const uint8_t*>(m->data.data()), m->data.size()));
            }
        }
        (void)ours.close();
        (void)theirs.close();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 100000) {
        return 0;
    }
    uint8_t mode = data[0];
    const uint8_t* p = data + 1;
    size_t n = size - 1;
    switch (mode & 3) {
        case 0:
            header(p, n, mode);
            break;
        case 1:
            utf8_case(p, n, mode);
            break;
        case 2:
            deflate_case(p, n, mode);
            break;
        default:
            connection(std::string(reinterpret_cast<const char*>(p), n), mode).wait();
            break;
    }
    return 0;
}
