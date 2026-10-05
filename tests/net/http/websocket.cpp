//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::websocket (RFC 6455, permessage-deflate RFC 7692). The frame codec,
// the mask, UTF-8 as it comes, the close payload, the handshake's key (the
// RFC's example), the deflate parameters and RFC 7692's own examples; the
// client and the server of the module against each other (text, bytes,
// empty and large messages, limits, subprotocols, fields, compression,
// refusals, the close handshake, a stop, keep-alive); cases in the manner
// of Autobahn's written from the RFC, against the server and the client by
// a peer of raw frames (fragmentation, control frames among fragments,
// invalid UTF-8 failing fast, reserved bits and opcodes, control frames too
// long or fragmented, masks on the wrong side, every close code); wss://,
// through an HTTP proxy and SOCKS5; interop with a peer written by hand in
// Go from the RFC with the standard library alone (go_websocket/main.go),
// and with curl where its build has ws (skipped otherwise).
#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/socks5_server.h"
#include "tests/net/http/proxy_server.h"
#include "sgcl/net/http/http.h"
#include "sgcl/compress.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;
using namespace sgcl::net::http::detail;
using sgcl_test::HttpProxyServer;
using sgcl_test::Socks5TestServer;
using ws = net::http::websocket;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string str(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    slice<const byte> bytes_of(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    std::string frame(uint8_t b0, const std::string& payload, bool masked, uint32_t mask = 0x11223344u) {
        uint8_t h[14];
        size_t n = write_ws_header(h, true, false, WsOpcode::text, payload.size(), masked, mask);
        h[0] = b0;
        std::string out(reinterpret_cast<const char*>(h), n);
        std::string p = payload;
        if (masked) {
            ws_mask(reinterpret_cast<uint8_t*>(p.data()), p.size(), mask, 0);
        }
        return out + p;
    }

    std::string close_payload(uint16_t code, const std::string& reason = "") {
        return std::string{char(code >> 8), char(code & 0xFF)} + reason;
    }

    // A peer of raw frames over a connection: what it reads kept in buf
    struct Raw {
        net::connection c;
        std::string buf;
        bool masks = false;

        bool fill() {
            vector<byte> b(65536);
            auto n = c.read(b.as_slice());
            if (!n || *n == 0) {
                return false;
            }
            buf.append(reinterpret_cast<const char*>(b.data()), *n);
            return true;
        }

        void send(const std::string& bytes) {
            (void)c.write(sgcl::string(bytes));
        }

        void send(uint8_t b0, const std::string& payload) {
            send(frame(b0, payload, masks));
        }

        // the next frame's first byte and payload; b0 0 at the end
        std::pair<uint8_t, std::string> next() {
            for (;;) {
                WsFrame f;
                const char* why = "";
                auto st = parse_ws_frame(reinterpret_cast<const uint8_t*>(buf.data()), buf.size(), !masks, true, f, why);
                if (st == WsParse::frame && buf.size() >= f.header + f.length) {
                    uint8_t b0 = uint8_t(buf[0]);
                    std::string p = buf.substr(f.header, size_t(f.length));
                    if (f.masked) {
                        ws_mask(reinterpret_cast<uint8_t*>(p.data()), p.size(), f.mask, 0);
                    }
                    buf.erase(0, f.header + size_t(f.length));
                    return {b0, p};
                }
                if (st == WsParse::invalid || !fill()) {
                    return {0, std::string()};
                }
            }
        }

        // the close frame's code, 0 when the connection ended without one
        int close_code() {
            for (;;) {
                auto [b0, p] = next();
                if (b0 == 0) {
                    return 0;
                }
                if ((b0 & 0x0F) == 0x8) {
                    return p.size() >= 2 ? (uint8_t(p[0]) << 8) | uint8_t(p[1]) : 1005;
                }
            }
        }

        // whether the connection ends (after what it still sends)
        bool ends() {
            for (int i = 0; i < 100; ++i) {
                if (!fill()) {
                    return true;
                }
            }
            return false;
        }
    };

    // What the server's handler saw: each message, and how it ended
    struct Seen {
        std::mutex lock;
        std::vector<std::string> messages;
        std::string ended;
        int close_status = 0;
        std::string close_reason;
        std::string subprotocol;
        bool compression = false;

        void add(const std::string& m) {
            std::lock_guard g(lock);
            messages.push_back(m);
        }

        std::string end() {
            std::lock_guard g(lock);
            return ended;
        }
    };

    // An echo server of the module: each message back as it came; the
    // handler's view kept in seen
    struct EchoServer {
        net::http::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        tracked_ptr<Seen> seen = make_tracked<Seen>();

        explicit EchoServer(const ws::options& o = ws::options(), const optional<net::tls::config>& tls = nullopt) {
            tracked_ptr<Seen> s = seen;
            tracked_ptr<ws::options> opts = make_tracked<ws::options>(o);
            srv.route("/ws", [s, opts](net::http::request req, net::http::response_writer w) -> async::task<> {
                auto c = co_await ws::async_accept(req, w, *opts);
                if (!c) {
                    std::lock_guard g(s->lock);
                    s->ended = "accept: " + text(c.error().message());
                    co_return;
                }
                {
                    std::lock_guard g(s->lock);
                    s->subprotocol = text(c->subprotocol());
                    s->compression = c->compression();
                }
                for (;;) {
                    auto m = co_await c->async_receive();
                    if (!m) {
                        std::lock_guard g(s->lock);
                        s->ended = text(m.error().message());
                        s->close_status = c->close_status();
                        s->close_reason = text(c->close_reason());
                        co_return;
                    }
                    s->add((m->binary ? "b:" : "t:") + str(m->data));
                    auto sent = m->binary ? co_await c->async_send(m->data.as_slice()) : co_await c->async_send(m->text());
                    if (!sent) {
                        co_return;
                    }
                }
            });
            listener = tls ? *net::tls::listen("127.0.0.1:0", *tls) : *net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(srv.async_serve(listener));
        }

        ~EchoServer() {
            srv.close();
            (void)serving.wait();
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        sgcl::string url(const char* scheme = "ws", const char* host = "127.0.0.1") const {
            return sgcl::string(std::string(scheme) + "://" + host + ":" + std::to_string(port()) + "/ws");
        }

        // a peer of raw frames, its handshake done (or the answer's head in buf)
        Raw raw(const std::string& extra = "", const std::string& path = "/ws", bool* upgraded = nullptr) {
            Raw r;
            r.c = *net::tcp::connect(listener.local_endpoint());
            r.masks = true;
            r.send("GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port()) +
                   "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n" + extra +
                   "\r\n");
            while (r.buf.find("\r\n\r\n") == std::string::npos && r.fill()) {
            }
            auto end = r.buf.find("\r\n\r\n");
            bool ok = r.buf.rfind("HTTP/1.1 101", 0) == 0;
            if (upgraded) {
                *upgraded = ok;
            }
            if (ok && end != std::string::npos) {
                r.buf.erase(0, end + 4);
            }
            return r;
        }
    };

    net::http::client direct() {
        net::http::client c;
        c.proxy = net::http::proxy();
        return c;
    }

    // A server of raw frames for the client's cases: the client's handshake
    // answered as given, the connection then the test's
    struct RawServer {
        net::listener listener = *net::tcp::listen("127.0.0.1:0");

        sgcl::string url() const {
            return sgcl::string("ws://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/x");
        }

        // the client's request read and answered: 101 with the accept of
        // its key and `extra` fields (or `answer` whole when given)
        Raw accept(const std::string& extra = "", const std::string& answer = "", std::string* request = nullptr, const std::string& behind = "") {
            Raw r;
            r.c = *listener.accept();
            while (r.buf.find("\r\n\r\n") == std::string::npos && r.fill()) {
            }
            auto end = r.buf.find("\r\n\r\n");
            std::string head = r.buf.substr(0, end + 4);
            r.buf.erase(0, end + 4);
            if (request) {
                *request = head;
            }
            auto k = head.find("Sec-WebSocket-Key: ");
            std::string key = head.substr(k + 19, head.find("\r\n", k) - k - 19);
            if (!answer.empty()) {
                r.send(answer);
            } else {
                r.send("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + text(ws_accept(key)) + "\r\n" + extra + "\r\n" + behind);
            }
            return r;
        }
    };

    bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    const std::string& go_peer() {
        static std::string path = [] {
            if (!have("go")) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_websocket/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_go_websocket";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    net::tls::config server_tls() {
        net::tls::config c;
        c.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        c.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
        return c;
    }
}

// --- the codec ----------------------------------------------------------------

TEST(WebSocketCodec_Tests, HeadersBothWays) {
    for (uint64_t len : {0ull, 1ull, 125ull, 126ull, 65535ull, 65536ull, 1ull << 32, (1ull << 63) - 1}) {
        for (bool masked : {false, true}) {
            uint8_t h[14];
            size_t n = write_ws_header(h, true, false, WsOpcode::binary, len, masked, 0xA1B2C3D4u);
            EXPECT_EQ(n, size_t(len < 126 ? 2 : len <= 0xFFFF ? 4 : 10) + (masked ? 4 : 0)) << len;
            for (size_t cut = 0; cut < n; ++cut) {
                WsFrame f;
                const char* why = "";
                EXPECT_EQ(parse_ws_frame(h, cut, masked, false, f, why), WsParse::more) << len << " " << cut;
            }
            WsFrame f;
            const char* why = "";
            ASSERT_EQ(parse_ws_frame(h, n, masked, false, f, why), WsParse::frame) << why;
            EXPECT_TRUE(f.fin);
            EXPECT_EQ(f.opcode, WsOpcode::binary);
            EXPECT_EQ(f.length, len);
            EXPECT_EQ(f.header, n);
            EXPECT_EQ(f.masked, masked);
            if (masked) {
                EXPECT_EQ(f.mask, 0xA1B2C3D4u);
            }
        }
    }
    // RFC 6455 §5.7's examples
    const uint8_t hello[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
    WsFrame f;
    const char* why = "";
    ASSERT_EQ(parse_ws_frame(hello, sizeof hello, false, false, f, why), WsParse::frame);
    EXPECT_EQ(f.length, 5u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(hello) + f.header, 5), "Hello");
    uint8_t masked[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
    ASSERT_EQ(parse_ws_frame(masked, sizeof masked, true, false, f, why), WsParse::frame);
    ws_mask(masked + f.header, 5, f.mask, 0);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(masked) + f.header, 5), "Hello");
    const uint8_t first[] = {0x01, 0x03, 0x48, 0x65, 0x6c}, last[] = {0x80, 0x02, 0x6c, 0x6f};
    ASSERT_EQ(parse_ws_frame(first, sizeof first, false, false, f, why), WsParse::frame);
    EXPECT_FALSE(f.fin);
    EXPECT_EQ(f.opcode, WsOpcode::text);
    ASSERT_EQ(parse_ws_frame(last, sizeof last, false, false, f, why), WsParse::frame);
    EXPECT_TRUE(f.fin);
    EXPECT_EQ(f.opcode, WsOpcode::continuation);
    const uint8_t big[] = {0x82, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    ASSERT_EQ(parse_ws_frame(big, sizeof big, false, false, f, why), WsParse::frame);
    EXPECT_EQ(f.length, 65536u);
}

TEST(WebSocketCodec_Tests, FramesThatBreakTheRules) {
    auto invalid = [](std::vector<uint8_t> b, bool expect_mask, bool deflate = false) {
        WsFrame f;
        const char* why = "";
        return parse_ws_frame(b.data(), b.size(), expect_mask, deflate, f, why) == WsParse::invalid;
    };
    EXPECT_TRUE(invalid({0xC1, 0x00}, false));          // RSV1 without the extension
    EXPECT_FALSE(invalid({0xC1, 0x00}, false, true));   // with it
    EXPECT_TRUE(invalid({0xA1, 0x00}, false, true));    // RSV2
    EXPECT_TRUE(invalid({0x91, 0x00}, false, true));    // RSV3
    for (uint8_t op : {3, 4, 5, 6, 7, 0xB, 0xC, 0xD, 0xE, 0xF}) {
        EXPECT_TRUE(invalid({uint8_t(0x80 | op), 0x00}, false)) << int(op);
    }
    EXPECT_TRUE(invalid({0x09, 0x00}, false));          // a ping fragmented
    EXPECT_TRUE(invalid({0x89, 0x7E, 0x00, 0x7E}, false));   // a ping of 126 bytes
    EXPECT_FALSE(invalid({0x89, 0x7D}, false));         // of 125
    EXPECT_TRUE(invalid({0xC9, 0x00}, false, true));    // a ping compressed
    EXPECT_TRUE(invalid({0x81, 0x00}, true));           // a client's frame unmasked
    EXPECT_TRUE(invalid({0x81, 0x80, 1, 2, 3, 4}, false));   // a server's frame masked
    EXPECT_TRUE(invalid({0x82, 0x7F, 0x80, 0, 0, 0, 0, 0, 0, 0}, false));   // the length's top bit
}

TEST(WebSocketCodec_Tests, TheMaskAtEveryOffsetAndAlignment) {
    std::string data(1000, '\0');
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = char(i * 7 + 3);
    }
    const uint32_t mask = 0xDEADBEEFu;
    const uint8_t key[4] = {0xEF, 0xBE, 0xAD, 0xDE};
    for (size_t start : {0, 1, 2, 3, 5, 8, 13}) {
        for (uint64_t offset : {0, 1, 2, 3, 6}) {
            std::string a = data;
            ws_mask(reinterpret_cast<uint8_t*>(a.data()) + start, a.size() - start, mask, offset);
            for (size_t i = start; i < a.size(); ++i) {
                ASSERT_EQ(uint8_t(a[i]), uint8_t(data[i] ^ key[(i - start + offset) % 4])) << start << " " << offset << " " << i;
            }
            ws_mask(reinterpret_cast<uint8_t*>(a.data()) + start, a.size() - start, mask, offset);
            EXPECT_EQ(a, data);
        }
    }
}

TEST(WebSocketCodec_Tests, Utf8AsItComes) {
    const std::vector<std::string> valid = {"", "plain", "\xC2\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80", "\xED\x9F\xBF", "\xEE\x80\x80", "\xF4\x8F\xBF\xBF",
                                            "\xE0\xA0\x80", "\xF0\x90\x80\x80", "\xCE\xBA\xE1\xBD\xB9\xCF\x83\xCE\xBC\xCE\xB5"};
    const std::vector<std::string> invalid = {"\x80", "\xBF", "\xC0\x80", "\xC1\xBF", "\xE0\x80\x80", "\xE0\x9F\xBF", "\xED\xA0\x80", "\xED\xBF\xBF",
                                              "\xF0\x80\x80\x80", "\xF0\x8F\xBF\xBF", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF", "\xFE",
                                              "\xC2\x41", "\xE2\x82\x41", "a\xF0\x9F\x98"};
    for (auto& v : valid) {
        EXPECT_TRUE(valid_utf8(reinterpret_cast<const uint8_t*>(v.data()), v.size())) << v;
        for (size_t cut = 0; cut <= v.size(); ++cut) {   // in two pieces, cut anywhere
            Utf8Check c;
            EXPECT_TRUE(c.feed(reinterpret_cast<const uint8_t*>(v.data()), cut) && c.feed(reinterpret_cast<const uint8_t*>(v.data()) + cut, v.size() - cut) && c.complete());
        }
    }
    for (auto& v : invalid) {
        EXPECT_FALSE(valid_utf8(reinterpret_cast<const uint8_t*>(v.data()), v.size())) << v;
    }
    // an open sequence at a fragment's end is no failure yet; at the message's end it is
    Utf8Check c;
    EXPECT_TRUE(c.feed(reinterpret_cast<const uint8_t*>("\xE2\x82"), 2));
    EXPECT_FALSE(c.complete());
    // the failure as soon as the byte comes, before the rest
    Utf8Check d;
    EXPECT_FALSE(d.feed(reinterpret_cast<const uint8_t*>("\xCE\xBA\xE1\xBD\xB9\xCF\x83\xCE\xBC\xCE\xB5\xED\xA0\x80"), 14));
    std::string long_ascii(1000, 'a');
    long_ascii[777] = char(0xFF);
    EXPECT_FALSE(valid_utf8(reinterpret_cast<const uint8_t*>(long_ascii.data()), long_ascii.size()));
}

TEST(WebSocketCodec_Tests, ClosePayloads) {
    uint16_t code = 0;
    std::string reason;
    EXPECT_TRUE(parse_ws_close(nullptr, 0, code, reason));
    EXPECT_EQ(code, 1005);
    auto ok = [&](const std::string& p) { return parse_ws_close(reinterpret_cast<const uint8_t*>(p.data()), p.size(), code, reason); };
    EXPECT_FALSE(ok(std::string(1, '\x03')));
    for (int c : {1000, 1001, 1002, 1003, 1007, 1008, 1009, 1010, 1011, 1012, 1013, 1014, 3000, 3999, 4000, 4999}) {
        EXPECT_TRUE(ok(close_payload(uint16_t(c), "why"))) << c;
        EXPECT_EQ(code, c);
        EXPECT_EQ(reason, "why");
    }
    for (int c : {0, 999, 1004, 1005, 1006, 1015, 1016, 1100, 2000, 2999, 5000, 65535}) {
        EXPECT_FALSE(ok(close_payload(uint16_t(c)))) << c;
    }
    EXPECT_FALSE(ok(close_payload(1000, "\xFF")));
}

TEST(WebSocketCodec_Tests, TheHandshakesKeyAndChecks) {
    EXPECT_EQ(text(ws_accept("dGhlIHNhbXBsZSBub25jZQ==")), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");   // RFC 6455 §1.3
    auto k1 = ws_key(), k2 = ws_key();
    EXPECT_EQ(k1.size(), 24u);
    EXPECT_NE(k1, k2);
    auto answer = [](std::vector<std::pair<const char*, const char*>> fields, int status = 101, std::vector<std::string> offered = {}, bool deflate = false) {
        net::http::headers h;
        for (auto& f : fields) {
            h.add(f.first, f.second);
        }
        WsAnswer a;
        const char* why = check_ws_answer(status, h, "dGhlIHNhbXBsZSBub25jZQ==", offered, deflate, a);
        return why ? std::string(why) : "ok " + text(a.subprotocol) + (a.deflate ? " deflate" : "");
    };
    const std::pair<const char*, const char*> up{"Upgrade", "websocket"}, conn{"Connection", "Upgrade"}, acc{"Sec-WebSocket-Accept", "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="};
    EXPECT_EQ(answer({up, conn, acc}), "ok ");
    EXPECT_EQ(answer({{"upgrade", "WebSocket"}, {"connection", "keep-alive, upgrade"}, acc}), "ok ");
    EXPECT_EQ(answer({up, conn, acc}, 200), "the status is not 101");
    EXPECT_EQ(answer({conn, acc}), "no Upgrade: websocket");
    EXPECT_EQ(answer({up, acc}), "no Connection: upgrade");
    EXPECT_EQ(answer({up, conn, {"Sec-WebSocket-Accept", "wrong"}}), "a Sec-WebSocket-Accept not of the key");
    EXPECT_EQ(answer({up, conn, acc, {"Sec-WebSocket-Protocol", "chat"}}, 101, {"chat", "other"}), "ok chat");
    EXPECT_EQ(answer({up, conn, acc, {"Sec-WebSocket-Protocol", "chat"}}), "a subprotocol not offered");
    EXPECT_EQ(answer({up, conn, acc, {"Sec-WebSocket-Protocol", "a, b"}}, 101, {"a", "b"}), "more than one subprotocol");
    EXPECT_EQ(answer({up, conn, acc, {"Sec-WebSocket-Extensions", "permessage-deflate"}}), "an extension not offered");
    EXPECT_EQ(answer({up, conn, acc, {"Sec-WebSocket-Extensions", "permessage-deflate; server_no_context_takeover"}}, 101, {}, true), "ok  deflate");
    EXPECT_EQ(answer({up, conn, acc, {"Sec-WebSocket-Extensions", "permessage-deflate; client_max_window_bits=10"}}, 101, {}, true),
              "client_max_window_bits below 15");
    EXPECT_EQ(answer({up, conn, acc, {"Sec-WebSocket-Extensions", "x-other"}}, 101, {}, true), "an extension not offered");

    auto request = [](std::vector<std::pair<const char*, const char*>> fields, const char* method = "GET", int minor = 1) {
        net::http::headers h;
        for (auto& f : fields) {
            h.add(f.first, f.second);
        }
        const char* why = "";
        return check_ws_request(method, minor, h, why);
    };
    const std::pair<const char*, const char*> key{"Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ=="}, ver{"Sec-WebSocket-Version", "13"};
    EXPECT_EQ(request({up, conn, key, ver}), 0);
    EXPECT_EQ(request({up, conn, key, ver}, "POST"), 405);
    EXPECT_EQ(request({up, conn, key, ver}, "GET", 0), 400);
    EXPECT_EQ(request({conn, key, ver}), 400);
    EXPECT_EQ(request({up, key, ver}), 400);
    EXPECT_EQ(request({up, conn, key}), 426);
    EXPECT_EQ(request({up, conn, key, {"Sec-WebSocket-Version", "8"}}), 426);
    EXPECT_EQ(request({up, conn, ver}), 400);
    EXPECT_EQ(request({up, conn, ver, {"Sec-WebSocket-Key", "c2hvcnQ="}}), 400);   // 5 bytes
    EXPECT_EQ(request({up, conn, ver, {"Sec-WebSocket-Key", "not base64!"}}), 400);

    auto origin = [](const char* o, std::vector<std::string> allowed = {}) {
        net::http::headers h;
        h.add("Host", "example.com:8080");
        if (o) {
            h.add("Origin", o);
        }
        return ws_origin_allowed(h, allowed);
    };
    EXPECT_TRUE(origin(nullptr));
    EXPECT_TRUE(origin("http://example.com:8080"));
    EXPECT_FALSE(origin("http://evil.example"));
    EXPECT_TRUE(origin("http://app.example", {"app.example"}));
    EXPECT_TRUE(origin("https://anything", {"*"}));
}

TEST(WebSocketCodec_Tests, DeflateParameters) {
    WsDeflateParams p;
    EXPECT_TRUE(parse_ws_deflate("permessage-deflate", p));
    EXPECT_FALSE(p.server_no_context_takeover || p.client_no_context_takeover);
    EXPECT_TRUE(parse_ws_deflate("permessage-deflate; server_no_context_takeover; client_no_context_takeover", p));
    EXPECT_TRUE(p.server_no_context_takeover && p.client_no_context_takeover);
    EXPECT_TRUE(parse_ws_deflate("permessage-deflate; client_max_window_bits", p));
    EXPECT_EQ(p.client_max_window_bits, 0);
    EXPECT_TRUE(parse_ws_deflate("permessage-deflate; server_max_window_bits=10; client_max_window_bits=\"12\"", p));
    EXPECT_EQ(p.server_max_window_bits, 10);
    EXPECT_EQ(p.client_max_window_bits, 12);
    EXPECT_FALSE(parse_ws_deflate("permessage-deflate; server_max_window_bits", p));    // a value it must have
    EXPECT_FALSE(parse_ws_deflate("permessage-deflate; server_max_window_bits=7", p));
    EXPECT_FALSE(parse_ws_deflate("permessage-deflate; server_max_window_bits=16", p));
    EXPECT_FALSE(parse_ws_deflate("permessage-deflate; server_max_window_bits=09", p));
    EXPECT_FALSE(parse_ws_deflate("permessage-deflate; x=1", p));
    EXPECT_FALSE(parse_ws_deflate("permessage-deflate; server_no_context_takeover; server_no_context_takeover", p));
    EXPECT_FALSE(parse_ws_deflate("permessage-deflate; server_no_context_takeover=1", p));
    EXPECT_FALSE(parse_ws_deflate("x-webkit-deflate-frame", p));
    // the server's choice: the first it can take
    auto chosen = choose_ws_deflate({"permessage-deflate; server_max_window_bits=10", "permessage-deflate; client_no_context_takeover"});
    ASSERT_TRUE(chosen);
    EXPECT_TRUE(chosen->client_no_context_takeover);
    EXPECT_EQ(ws_deflate_answer(*chosen), "permessage-deflate; client_no_context_takeover");
    EXPECT_FALSE(choose_ws_deflate({"permessage-deflate; server_max_window_bits=9"}));
    EXPECT_TRUE(choose_ws_deflate({"permessage-deflate; server_max_window_bits=15"}));
}

TEST(WebSocketCodec_Tests, Rfc7692Examples) {
    auto inflate = [](WsDeflate& d, std::vector<uint8_t> in) {
        vector<byte> out;
        bool too_large = false;
        bool ok = d.decompress(in.data(), in.size(), out, 1 << 20, too_large);
        return ok ? str(out) : std::string("<failed>");
    };
    WsDeflate d(false, false);
    EXPECT_EQ(inflate(d, {0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00}), "Hello");   // §7.2.3.1
    EXPECT_EQ(inflate(d, {0xf2, 0x00, 0x11, 0x00, 0x00}), "Hello");               // §7.2.3.2: the window shared
    WsDeflate stored(false, false);
    EXPECT_EQ(inflate(stored, {0x00, 0x05, 0x00, 0xfa, 0xff, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x00}), "Hello");   // §7.2.3.3
    WsDeflate final_block(false, false);
    EXPECT_EQ(inflate(final_block, {0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x00}), "Hello");   // §7.2.3.4: BFINAL set
    EXPECT_EQ(inflate(final_block, {0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00}), "Hello");         // and a message after it
    WsDeflate corrupt(false, false);
    EXPECT_EQ(inflate(corrupt, {0xff, 0xff, 0xff}), "<failed>");
    // the module's own both ways, the window kept and not
    for (bool reset : {false, true}) {
        WsDeflate out(reset, false), in(false, reset);
        std::string msg = "a message that repeats, a message that repeats, a message that repeats";
        std::vector<uint8_t> c1, c2;
        out.compress(reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), c1);
        out.compress(reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), c2);
        if (!reset) {
            EXPECT_LT(c2.size(), c1.size());   // the second refers to the first
        } else {
            EXPECT_EQ(c2, c1);
        }
        EXPECT_EQ(inflate(in, c1), msg);
        EXPECT_EQ(inflate(in, c2), msg);
        std::vector<uint8_t> empty;
        out.compress(nullptr, 0, empty);
        EXPECT_EQ(inflate(in, empty), "");
    }
    // flate::compress's whole stream (a last block) read as a message
    vector<byte> whole = compress::flate::compress("whole stream");
    WsDeflate any(false, false);
    EXPECT_EQ(inflate(any, std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(whole.data()), reinterpret_cast<const uint8_t*>(whole.data()) + whole.size())),
              "whole stream");
    // the limit on what it decompresses to
    WsDeflate big_out(false, false), big_in(false, false);
    std::string zeros(100000, '0');
    std::vector<uint8_t> c;
    big_out.compress(reinterpret_cast<const uint8_t*>(zeros.data()), zeros.size(), c);
    vector<byte> out;
    bool too_large = false;
    EXPECT_FALSE(big_in.decompress(c.data(), c.size(), out, 50000, too_large));
    EXPECT_TRUE(too_large);
}

// --- the module's client and server -------------------------------------------

TEST(WebSocket_Tests, EchoOfTextAndBytes) {
    EchoServer server;
    auto c = direct().websocket(server.url());
    ASSERT_TRUE(c) << text(c.error().message());
    ASSERT_TRUE(c->send("hello"));
    auto m = c->receive();
    ASSERT_TRUE(m) << text(m.error().message());
    EXPECT_FALSE(m->binary);
    EXPECT_EQ(text(m->text()), "hello");
    std::string bin = std::string("\0\1\2\xFF", 4);
    ASSERT_TRUE(c->send(bytes_of(bin)));
    m = c->receive();
    EXPECT_TRUE(m->binary);
    EXPECT_EQ(str(m->data), bin);
    ASSERT_TRUE(c->send(""));   // empty
    EXPECT_EQ(text(c->receive()->text()), "");
    ASSERT_TRUE(c->send(slice<const byte>()));
    m = c->receive();
    EXPECT_TRUE(m->binary && m->data.empty());
    for (size_t n : {125, 126, 127, 65535, 65536, 65537, 5000000}) {   // every length encoding, a large one
        std::string s(n, 'x');
        for (size_t i = 0; i < n; i += 997) {
            s[i] = char('a' + i % 26);
        }
        ASSERT_TRUE(c->send(sgcl::string(s)));
        auto r = c->receive();
        ASSERT_TRUE(r) << n;
        EXPECT_EQ(str(r->data), s) << n;
    }
    EXPECT_EQ(text(c->subprotocol()), "");
    EXPECT_FALSE(c->compression());
    EXPECT_FALSE(c->is_closed());
    EXPECT_EQ(c->close_status(), 0);
    ws copy = *c;   // the same connection
    EXPECT_TRUE(copy == *c);
    ASSERT_TRUE(c->close());
    EXPECT_TRUE(c->is_closed());
    EXPECT_EQ(c->close_status(), 1000);   // the server's echo
    auto after = c->send("late");
    ASSERT_FALSE(after);
    EXPECT_EQ(after.error().code(), io::errc::closed);
    ASSERT_FALSE(c->receive());
    EXPECT_TRUE(c->close());   // twice: nothing more
    for (int i = 0; i < 100 && server.seen->end().empty(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_EQ(server.seen->end(), "websocket 1000: WebSocket closed by the peer");
    ws none;
    EXPECT_FALSE(none);
}

TEST(WebSocket_Tests, CloseCodesAndReasons) {
    EchoServer server;
    auto c = direct().websocket(server.url());
    ASSERT_TRUE(c);
    EXPECT_EQ(c->close(999, "x").error().code(), std::errc::invalid_argument);
    EXPECT_EQ(c->close(1005, "x").error().code(), std::errc::invalid_argument);
    EXPECT_EQ(c->close(4000, sgcl::string(std::string(124, 'r'))).error().code(), std::errc::invalid_argument);
    EXPECT_EQ(c->close(4000, sgcl::string(std::string("\xFF"))).error().code(), std::errc::invalid_argument);
    EXPECT_FALSE(c->is_closed());   // nothing sent for those
    ASSERT_TRUE(c->close(4000, sgcl::string(std::string(123, 'r'))));
    for (int i = 0; i < 100 && server.seen->end().empty(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    std::lock_guard g(server.seen->lock);
    EXPECT_EQ(server.seen->close_status, 4000);
    EXPECT_EQ(server.seen->close_reason, std::string(123, 'r'));
}

TEST(WebSocket_Tests, SubprotocolsFieldsAndRefusals) {
    ws::options so;
    so.subprotocols = {sgcl::string("v2"), sgcl::string("v1")};
    so.headers.set("X-Server", "yes");
    so.origins = {sgcl::string("app.example")};
    EchoServer server(so);
    ws::options co;
    co.subprotocols = {sgcl::string("v1"), sgcl::string("v2")};
    co.headers.set("X-Client", "1");
    auto c = direct().websocket(server.url(), co);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(text(c->subprotocol()), "v2");   // the server's preference
    EXPECT_TRUE(c->send("x"));
    EXPECT_TRUE(c->receive());
    {
        std::lock_guard g(server.seen->lock);
        EXPECT_EQ(server.seen->subprotocol, "v2");
    }
    (void)c->close();

    // none in common: none chosen
    ws::options other;
    other.subprotocols = {sgcl::string("v9")};
    auto none = direct().websocket(server.url(), other);
    ASSERT_TRUE(none);
    EXPECT_EQ(text(none->subprotocol()), "");
    (void)none->close();

    // origins: its own host, one allowed, one not
    ws::options from;
    from.headers.set("Origin", "https://app.example");
    EXPECT_TRUE(direct().websocket(server.url(), from));
    from.headers.set("Origin", sgcl::string("http://127.0.0.1:" + std::to_string(server.port())));
    EXPECT_TRUE(direct().websocket(server.url(), from));
    from.headers.set("Origin", "https://evil.example");
    auto refused = direct().websocket(server.url(), from);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::websocket_handshake);
    EXPECT_NE(text(refused.error().message()).find("403"), std::string::npos) << text(refused.error().message());

    // requests that are not an upgrade, refused by the server
    auto plain = direct().get(server.url("http"));
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->status(), 400);
    (void)plain->text();
    net::http::request v8("GET", server.url("http"));
    v8.set_header("Upgrade", "websocket").set_header("Connection", "Upgrade").set_header("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==").set_header("Sec-WebSocket-Version", "8");
    auto old = direct().send(v8);
    ASSERT_TRUE(old);
    EXPECT_EQ(old->status(), 426);
    EXPECT_EQ(old->header("Sec-WebSocket-Version"), "13");
    (void)old->text();
    auto post = direct().post(server.url("http"), "text/plain", "x");
    EXPECT_EQ(post->status(), 405);
    (void)post->text();
}

TEST(WebSocket_Tests, AnHttp2RequestIsRefused) {
    net::http::server srv;
    srv.h2c = true;
    tracked_ptr<std::string> got = make_tracked<std::string>();
    srv.route("/ws", [got](net::http::request req, net::http::response_writer w) -> async::task<> {
        auto c = co_await ws::async_accept(req, w);
        *got = c ? "accepted" : text(c.error().message());
    });
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::http::client h2 = direct();
    h2.h2c = true;
    auto res = h2.get(sgcl::string("http://127.0.0.1:" + std::to_string(l.local_endpoint().port()) + "/ws"));
    ASSERT_TRUE(res);
    EXPECT_EQ(text(res->proto()), "HTTP/2.0");
    EXPECT_EQ(res->status(), 505);
    (void)res->text();
    EXPECT_EQ(*got, "websocket an HTTP/2 request (RFC 8441 is not served): WebSocket handshake failed");
    srv.close();
    (void)serving.wait();
}

TEST(WebSocket_Tests, Compression) {
    ws::options so;
    so.compression = true;
    EchoServer server(so);
    ws::options co;
    co.compression = true;
    auto c = direct().websocket(server.url(), co);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_TRUE(c->compression());
    std::string repeated;
    for (int i = 0; i < 2000; ++i) {
        repeated += "compress me " + std::to_string(i % 10) + " ";
    }
    for (int i = 0; i < 5; ++i) {   // the window kept from one message to the next
        ASSERT_TRUE(c->send(sgcl::string(repeated)));
        auto m = c->receive();
        ASSERT_TRUE(m) << text(m.error().message());
        EXPECT_EQ(str(m->data), repeated);
    }
    std::string bin(300000, '\0');
    for (size_t i = 0; i < bin.size(); ++i) {
        bin[i] = char((i * 2654435761u) >> 24);   // hardly compressible
    }
    ASSERT_TRUE(c->send(bytes_of(bin)));
    EXPECT_EQ(str(c->receive()->data), bin);
    ASSERT_TRUE(c->send(""));
    EXPECT_EQ(text(c->receive()->text()), "");
    {
        std::lock_guard g(server.seen->lock);
        EXPECT_TRUE(server.seen->compression);
    }
    (void)c->close();
    // offered and not taken (the server's default), and taken but not offered
    EchoServer plain;
    auto p = direct().websocket(plain.url(), co);
    ASSERT_TRUE(p);
    EXPECT_FALSE(p->compression());
    EXPECT_TRUE(p->send("plain") && p->receive());
    auto q = direct().websocket(server.url());
    ASSERT_TRUE(q);
    EXPECT_FALSE(q->compression());
    // a raw client: what it sends compressed is read, the bytes on the wire are compressed
    bool upgraded = false;
    auto r = server.raw("Sec-WebSocket-Extensions: permessage-deflate; client_no_context_takeover\r\n", "/ws", &upgraded);
    ASSERT_TRUE(upgraded);
    std::vector<uint8_t> hello = {0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00};
    std::string payload(hello.begin(), hello.end());
    r.send(frame(0xC1, payload, true));   // RSV1, text
    auto [b0, back] = r.next();
    EXPECT_EQ(b0, 0xC1);   // compressed back
    WsDeflate d(false, false);
    vector<byte> out;
    bool too_large = false;
    ASSERT_TRUE(d.decompress(reinterpret_cast<const uint8_t*>(back.data()), back.size(), out, 1000, too_large));
    EXPECT_EQ(str(out), "Hello");
    r.send(frame(0xC1, "\xFF\xFF\xFF", true));   // not DEFLATE
    EXPECT_EQ(r.close_code(), 1007);
}

TEST(WebSocket_Tests, MessageLimits) {
    ws::options so;
    so.max_message_bytes = 1000;
    EchoServer server(so);
    auto c = direct().websocket(server.url());
    ASSERT_TRUE(c);
    ASSERT_TRUE(c->send(sgcl::string(std::string(1000, 'a'))));   // at the limit
    EXPECT_EQ(c->receive()->data.size(), 1000u);
    ASSERT_TRUE(c->send(sgcl::string(std::string(1001, 'a'))));
    auto m = c->receive();
    ASSERT_FALSE(m);
    EXPECT_EQ(m.error().code(), net::errc::websocket_closed);
    EXPECT_EQ(c->close_status(), 1009);
    // fragments that together pass it
    bool upgraded = false;
    auto r = server.raw("", "/ws", &upgraded);
    ASSERT_TRUE(upgraded);
    r.send(0x01, std::string(600, 'a'));
    r.send(0x80, std::string(600, 'a'));
    EXPECT_EQ(r.close_code(), 1009);
    // the client's limit
    ws::options co;
    co.max_message_bytes = 10;
    EchoServer big;
    auto small = direct().websocket(big.url(), co);
    ASSERT_TRUE(small);
    ASSERT_TRUE(small->send("more than ten bytes"));
    EXPECT_EQ(small->receive().error().code(), net::errc::body_too_large);
}

TEST(WebSocket_Tests, ManySendersOneReceiver) {
    EchoServer server;
    auto c = direct().websocket(server.url());
    ASSERT_TRUE(c);
    auto sender = [](ws c, int k) -> async::task<int> {
        int ok = 0;
        for (int i : range(50)) {
            ok += (bool)co_await c.async_send(sgcl::string("task " + std::to_string(k) + " message " + std::to_string(i)));
        }
        co_return ok;
    };
    auto receiver = [](ws c, int n) -> async::task<int> {
        int got = 0;
        for (int i : range(n)) {
            (void)i;
            auto m = co_await c.async_receive();
            if (!m || text(m->text()).rfind("task ", 0) != 0) {
                break;
            }
            ++got;
        }
        co_return got;
    };
    auto r = async::spawn(receiver(*c, 8 * 50));
    vector<async::task<int>> senders;
    for (int k : range(8)) {
        senders.push_back(async::spawn(sender(*c, k)));
    }
    int sent = 0;
    for (auto& s : senders) {
        sent += s.wait();
    }
    EXPECT_EQ(sent, 400);
    EXPECT_EQ(r.wait(), 400);
    (void)c->close();
}

TEST(WebSocket_Tests, ACloseWhileAReceiveWaits) {
    EchoServer server;
    auto c = direct().websocket(server.url());
    ASSERT_TRUE(c);
    auto receiver = [](ws c) -> async::task<std::string> {
        auto m = co_await c.async_receive();
        co_return m ? std::string("message") : text(m.error().message());
    };
    auto r = async::spawn(receiver(*c));
    std::this_thread::sleep_for(50ms);
    auto t0 = std::chrono::steady_clock::now();
    ASSERT_TRUE(c->close(1001, "leaving"));
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 3s);   // the waiting receive read the server's close
    EXPECT_EQ(r.wait(), "websocket connection: stream closed");
    EXPECT_EQ(c->close_status(), 1001);
}

TEST(WebSocket_Tests, TheStopAndKeepAlive) {
    EchoServer server;
    async::stop_source source;
    ws::options co;
    co.stop = source.token();
    auto c = direct().websocket(server.url(), co);
    ASSERT_TRUE(c);
    EXPECT_TRUE(c->send("x") && c->receive());
    source.request_stop();
    auto m = c->receive();
    ASSERT_FALSE(m);
    EXPECT_EQ(m.error().code(), std::errc::operation_canceled);
    for (int i = 0; i < 100 && server.seen->end().empty(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    {
        std::lock_guard g(server.seen->lock);
        EXPECT_EQ(server.seen->close_status, 1001);   // going away
    }

    // keep-alive: a peer that answers pings keeps it; one that is silent loses it
    ws::options alive;
    alive.ping_interval = 50ms;
    auto k = direct().websocket(server.url(), alive);
    ASSERT_TRUE(k);
    auto reading = [](ws c) -> async::task<std::string> {
        auto m = co_await c.async_receive();
        co_return m ? std::string("message") : text(m.error().message());
    };
    auto waiting = async::spawn(reading(*k));
    std::this_thread::sleep_for(400ms);
    EXPECT_FALSE(k->is_closed());   // the server's pongs heard
    (void)k->close();
    (void)waiting.wait();

    RawServer silent;
    auto client_task = async::spawn(direct().async_websocket(silent.url(), alive));
    Raw peer = silent.accept();
    auto quiet = client_task.wait();
    ASSERT_TRUE(quiet);
    auto lost = async::spawn(reading(*quiet));
    auto t0 = std::chrono::steady_clock::now();
    std::string why = lost.wait();
    EXPECT_NE(why.find("Operation timed out"), std::string::npos) << why;
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 3s);
    auto [b0, p] = peer.next();
    EXPECT_EQ(b0, 0x89);   // the pings it sent, masked
    EXPECT_EQ(p, "");
}

TEST(WebSocket_Tests, PingsArePongedAndConnectRefusals) {
    RawServer rs;
    auto client_task = async::spawn(direct().async_websocket(rs.url()));
    std::string request;
    Raw peer = rs.accept("", "", &request);
    auto c = client_task.wait();
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_NE(request.find("GET /x HTTP/1.1\r\n"), std::string::npos);
    EXPECT_NE(request.find("Upgrade: websocket\r\n"), std::string::npos);
    EXPECT_NE(request.find("Sec-WebSocket-Version: 13\r\n"), std::string::npos);
    EXPECT_EQ(request.find("Sec-WebSocket-Extensions"), std::string::npos);   // not offered by default
    peer.send(0x89, "are you there");
    peer.send(0x81, "after the ping");
    auto m = c->receive();   // the ping answered on the way
    ASSERT_TRUE(m);
    EXPECT_EQ(text(m->text()), "after the ping");
    auto [b0, p] = peer.next();
    EXPECT_EQ(b0, 0x8A);
    EXPECT_EQ(p, "are you there");
    ASSERT_TRUE(c->ping(bytes_of("mine")));
    auto [b1, p1] = peer.next();
    EXPECT_EQ(b1, 0x89);
    EXPECT_EQ(p1, "mine");
    EXPECT_EQ(c->ping(bytes_of(std::string(126, 'p'))).error().code(), std::errc::invalid_argument);
    peer.send(0x8A, "unasked pong");   // a pong nobody asked for is fine
    peer.send(0x82, "x");
    EXPECT_TRUE(c->receive());

    // answers that are not an upgrade
    auto refused = [&](const std::string& answer) {
        auto t = async::spawn(direct().async_websocket(rs.url()));
        Raw p2 = rs.accept("", answer);
        auto r = t.wait();
        return r ? std::string("connected") : text(r.error().message());
    };
    EXPECT_NE(refused("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n").find("404: the status is not 101"), std::string::npos);
    EXPECT_NE(refused("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: bad\r\n\r\n")
                  .find("a Sec-WebSocket-Accept not of the key"),
              std::string::npos);
    EXPECT_NE(refused("garbage\r\n\r\n").find("malformed HTTP response"), std::string::npos);
    // schemes and URLs
    EXPECT_EQ(ws::connect("ftp://x/").error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(ws::connect("not a url").error().code(), net::errc::invalid_url);
    auto gone = RawServer();
    auto url = gone.url();
    gone.listener.close();
    EXPECT_EQ(direct().websocket(url).error().code(), std::errc::connection_refused);
    // the first frames behind the 101, in its write: read, not lost with the head
    auto t = async::spawn(direct().async_websocket(rs.url()));
    Raw p3 = rs.accept("", "", nullptr, frame(0x81, "first", false) + frame(0x82, "second", false));
    auto early = t.wait();
    ASSERT_TRUE(early);
    EXPECT_EQ(text(early->receive()->text()), "first");
    EXPECT_EQ(str(early->receive()->data), "second");
    (void)p3;
}

// --- Autobahn's manner: the server -------------------------------------------

TEST(WebSocketCases_Tests, TheServerOnFramesOfEveryKind) {
    EchoServer server;
    auto upgraded = [&]() {
        bool ok = false;
        auto r = server.raw("", "/ws", &ok);
        EXPECT_TRUE(ok);
        return r;
    };
    {   // fragmentation, a ping among the fragments (5.6, 5.15)
        auto r = upgraded();
        r.send(0x01, "frag");
        r.send(0x89, "ping");
        r.send(0x00, "men");
        r.send(0x80, "ted");
        auto [b0, p] = r.next();
        EXPECT_EQ(b0, 0x8A);
        EXPECT_EQ(p, "ping");
        auto [b1, m] = r.next();
        EXPECT_EQ(b1, 0x81);
        EXPECT_EQ(m, "fragmented");
        r.send(0x88, close_payload(1000));
        EXPECT_EQ(r.close_code(), 1000);
        EXPECT_TRUE(r.ends());
    }
    {   // fragments of one byte, a text cut inside a character
        auto r = upgraded();
        std::string euro = "\xE2\x82\xAC";
        r.send(0x01, euro.substr(0, 1));
        r.send(0x00, euro.substr(1, 1));
        r.send(0x80, euro.substr(2, 1));
        EXPECT_EQ(r.next().second, euro);
    }
    auto failed = [&](std::vector<std::string> frames) {
        auto r = upgraded();
        for (auto& f : frames) {
            r.send(f);
        }
        return r.close_code();
    };
    EXPECT_EQ(failed({frame(0x80, "x", true)}), 1002);                                   // a continuation with no message
    EXPECT_EQ(failed({frame(0x01, "a", true), frame(0x81, "b", true)}), 1002);           // a message inside another
    EXPECT_EQ(failed({frame(0x83, "", true)}), 1002);                                    // a reserved opcode
    EXPECT_EQ(failed({frame(0x8B, "", true)}), 1002);
    EXPECT_EQ(failed({frame(0xC1, "x", true)}), 1002);                                   // RSV1 not agreed
    EXPECT_EQ(failed({frame(0x91, "x", true)}), 1002);                                   // RSV3
    EXPECT_EQ(failed({frame(0x89, std::string(126, 'p'), true)}), 1002);                 // a ping of 126 bytes
    EXPECT_EQ(failed({frame(0x09, "p", true)}), 1002);                                   // a ping fragmented
    EXPECT_EQ(failed({frame(0x81, "unmasked", false)}), 1002);                           // a client's frame unmasked
    EXPECT_EQ(failed({frame(0x81, "\xCE\xBA\xE1\xBD\xB9\xED\xA0\x80", true)}), 1007);     // invalid UTF-8
    // fail fast: the first fragment's bad byte, the rest never sent
    EXPECT_EQ(failed({frame(0x01, "ok \xF5", true)}), 1007);
    EXPECT_EQ(failed({frame(0x01, "\xE2\x82", true), frame(0x80, "", true)}), 1007);    // a sequence open at the end
    // close frames (7.x)
    EXPECT_EQ(failed({frame(0x88, "", true)}), 1005);                     // no code: echoed without one
    EXPECT_EQ(failed({frame(0x88, "\x03", true)}), 1002);                 // one byte
    for (int code : {1000, 1001, 1002, 1003, 1007, 1008, 1009, 1010, 1011, 3000, 3999, 4000, 4999}) {
        EXPECT_EQ(failed({frame(0x88, close_payload(uint16_t(code), "r"), true)}), code) << code;
    }
    for (int code : {0, 999, 1004, 1005, 1006, 1015, 1016, 2000, 2999, 5000, 65535}) {
        EXPECT_EQ(failed({frame(0x88, close_payload(uint16_t(code)), true)}), 1002) << code;
    }
    EXPECT_EQ(failed({frame(0x88, close_payload(1000, "\xFF"), true)}), 1007);   // a reason not UTF-8
    // after the close nothing more is read: a frame behind it changes nothing
    EXPECT_EQ(failed({frame(0x88, close_payload(1000), true), frame(0x81, "after", true)}), 1000);
    // a length with its top bit
    auto r = upgraded();
    r.send(std::string("\x82\xFF\x80\0\0\0\0\0\0\0\x11\x22\x33\x44", 14));
    EXPECT_EQ(r.close_code(), 1002);
}

// --- and the client -----------------------------------------------------------

TEST(WebSocketCases_Tests, TheClientOnFramesOfEveryKind) {
    RawServer rs;
    auto connected = [&]() {
        auto t = async::spawn(direct().async_websocket(rs.url()));
        Raw p = rs.accept();
        auto c = t.wait();
        EXPECT_TRUE(c);
        return std::make_pair(std::move(p), *c);
    };
    {
        auto [peer, c] = connected();
        peer.send(0x01, "a");
        peer.send(0x89, "");
        peer.send(0x80, "b");
        auto m = c.receive();
        ASSERT_TRUE(m);
        EXPECT_EQ(text(m->text()), "ab");
        EXPECT_EQ(peer.next().first, 0x8A);
    }
    auto failed = [&](const std::string& bytes) {
        auto [peer, c] = connected();
        peer.send(bytes);
        auto m = c.receive();
        int code = peer.close_code();
        return std::make_pair(m ? std::string("message") : text(m.error().message()), code);
    };
    EXPECT_EQ(failed(frame(0x81, "masked", true)).second, 1002);   // a server's frame masked
    EXPECT_EQ(failed(frame(0x84, "", false)).second, 1002);
    EXPECT_EQ(failed(frame(0x81, "\xC0\x80", false)).second, 1007);
    auto too_long = failed(frame(0x8A, std::string(126, 'x'), false));
    EXPECT_EQ(too_long.second, 1002);
    EXPECT_EQ(too_long.first, "websocket a control frame longer than 125 bytes: WebSocket protocol violation");
    // the server closes: the client echoes, its receive says so
    auto [peer, c] = connected();
    peer.send(0x88, close_payload(4001, "server says bye"));
    auto m = c.receive();
    ASSERT_FALSE(m);
    EXPECT_EQ(m.error().code(), net::errc::websocket_closed);
    EXPECT_EQ(c.close_status(), 4001);
    EXPECT_EQ(text(c.close_reason()), "server says bye");
    EXPECT_EQ(peer.close_code(), 4001);
    // the server ends the connection without a close: the error of the read
    auto [peer2, c2] = connected();
    (void)peer2.c.close();
    auto cut = c2.receive();
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), io::errc::unexpected_eof);
}

// --- wss://, proxies ------------------------------------------------------------

TEST(WebSocket_Tests, OverTlsAndThroughProxies) {
    EchoServer secure(ws::options(), server_tls());
    net::http::client c = direct();
    c.tls.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
    auto s = c.websocket(secure.url("wss", "localhost"));   // ALPN offers h2, the client asks for http/1.1 alone
    ASSERT_TRUE(s) << text(s.error().message());
    EXPECT_TRUE(s->send("over tls"));
    EXPECT_EQ(text(s->receive()->text()), "over tls");
    auto st = net::tls::state_of(s->connection());
    ASSERT_TRUE(st);
    EXPECT_EQ(text(st->alpn), "http/1.1");
    (void)s->close();
    net::http::client strict = direct();   // the system's roots: refused
    EXPECT_FALSE(strict.websocket(secure.url("wss", "localhost")));

    EchoServer plain;
    auto proxy = HttpProxyServer::start();
    net::http::client via = direct();
    via.proxy = net::http::proxy(proxy->url());
    auto p = via.websocket(plain.url());   // ws:// through a CONNECT tunnel, never forwarded
    ASSERT_TRUE(p) << text(p.error().message());
    EXPECT_TRUE(p->send("tunnelled"));
    EXPECT_EQ(text(p->receive()->text()), "tunnelled");
    EXPECT_EQ(proxy->lines().back(), "CONNECT 127.0.0.1:" + std::to_string(plain.port()) + " HTTP/1.1");
    (void)p->close();
    via.tls.roots = c.tls.roots;
    auto ps = via.websocket(secure.url("wss", "localhost"));
    ASSERT_TRUE(ps) << text(ps.error().message());
    EXPECT_TRUE(ps->send("wss tunnelled") && ps->receive());
    (void)ps->close();
    proxy->close();

    auto socks = Socks5TestServer::start();
    net::http::client sv = direct();
    sv.proxy = net::http::proxy(sgcl::string("socks5h://" + text(socks->address())));
    auto x = sv.websocket(plain.url("ws", "localhost"));
    ASSERT_TRUE(x) << text(x.error().message());
    EXPECT_TRUE(x->send("socks") && x->receive());
    (void)x->close();
    socks->close();
}

TEST(WebSocket_Tests, TheTaskFormsAndTheDefaultClient) {
    EchoServer server;
    auto run = [](sgcl::string url) -> async::task<std::string> {
        auto c = co_await ws::async_connect(url);
        if (!c) {
            co_return text(c.error().message());
        }
        (void)co_await c->async_send("in a task");
        auto m = co_await c->async_receive();
        (void)co_await c->async_ping();
        (void)co_await c->async_close(1000, "done");
        co_return m ? text(m->text()) : text(m.error().message());
    };
    EXPECT_EQ(async::spawn(run(server.url())).wait(), "in a task");
    auto blocking = ws::connect(server.url());   // the process's client (the environment's proxy: none in the tests)
    if (blocking) {
        EXPECT_TRUE(blocking->send("b") && blocking->receive());
        (void)blocking->close();
    }
}

// --- interop: Go and curl ---------------------------------------------------------

TEST(WebSocketInterop_Tests, GoPeerBothWays) {
    if (go_peer().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    for (bool deflate : {false, true}) {
        FILE* p = popen(("'" + go_peer() + "' server" + (deflate ? " deflate" : "")).c_str(), "r");
        ASSERT_TRUE(p);
        char line[64] = {};
        ASSERT_TRUE(fgets(line, sizeof(line), p));
        int port = std::atoi(line + 5);
        ASSERT_GT(port, 0);
        std::string base = "127.0.0.1:" + std::to_string(port);
        ws::options o;
        o.subprotocols = {sgcl::string("chat")};
        o.compression = deflate;
        auto c = direct().websocket(sgcl::string("ws://" + base + "/echo"), o);
        ASSERT_TRUE(c) << text(c.error().message());
        EXPECT_EQ(text(c->subprotocol()), "chat");
        EXPECT_EQ(c->compression(), deflate);
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(c->send(sgcl::string("hello go " + std::to_string(i))));
            EXPECT_EQ(text(c->receive()->text()), "hello go " + std::to_string(i));
        }
        std::string big(200000, 'g');
        for (size_t i = 0; i < big.size(); i += 101) {
            big[i] = char(i);
        }
        ASSERT_TRUE(c->send(bytes_of(big)));
        EXPECT_EQ(str(c->receive()->data), big);
        ASSERT_TRUE(c->send("frag:4"));   // Go fragments its answer, a ping among the frames
        EXPECT_EQ(text(c->receive()->text()), "fragmented");
        ASSERT_TRUE(c->send("close:4002"));
        auto m = c->receive();
        ASSERT_FALSE(m);
        EXPECT_EQ(c->close_status(), 4002);
        EXPECT_EQ(text(c->close_reason()), "bye");
        (void)direct().get(sgcl::string("http://" + base + "/quit"));
        pclose(p);
    }
    // Go's client against this server
    EchoServer server([] {
        ws::options o;
        o.subprotocols = {sgcl::string("chat")};
        return o;
    }());
    auto out = run("'" + go_peer() + "' client " + text(server.url()));
    EXPECT_EQ(out, "protocol: chat\ntext: hello\nbinary: 3 same=true\ntext: in three frames\nbinary: 100000 same=true\nclose: 1000\n");
}

TEST(WebSocketInterop_Tests, Curl) {
    if (!have("curl") || run("curl --version").find(" ws ") == std::string::npos) {
        GTEST_SKIP() << "curl without ws on this machine";
    }
    EchoServer server;
    // curl's ws support sends what stdin holds and prints what comes back
    auto out = run("printf 'hi' | curl -s --max-time 3 --no-buffer -T - " + text(server.url()));
    EXPECT_NE(out.find("hi"), std::string::npos);
}
