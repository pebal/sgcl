//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http/2: the server's connection machine, pure (sgcl/net/http/detail/h2/
// connection.h), fed frames written by a client made here (FrameWriter and
// HPACK's Encoder) and read back from its output. One test per rule of
// RFC 9113 it keeps, named by the section: the preface, SETTINGS and its
// acknowledgement, the states of a stream (§5.1), identifiers, the limit
// of concurrent streams, flow control both ways (§5.2, §6.9), field blocks
// across CONTINUATION, trailers, RST_STREAM, PING, GOAWAY; each error with
// the code and the reach (GOAWAY or RST_STREAM) the RFC names. Then one
// test per attack the machine limits: rapid reset (CVE-2023-44487), the
// CONTINUATION flood (CVE-2023-45288), the HPACK bomb (431), control-frame
// and empty-frame floods, a window held at zero.
#include "tests/types.h"
#include "sgcl/net/http/detail/h2/connection.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace sgcl;
using namespace sgcl::net::http::detail::h2;

namespace {
    // What the machine told the server (std types: nothing managed kept
    // past a call)
    struct Sink {
        struct Request {
            uint32_t id;
            std::string path;
            bool end;
            bool start;
        };
        std::vector<Request> requests;
        std::vector<uint32_t> started;
        std::vector<uint32_t> trailers;
        std::vector<std::pair<uint32_t, ErrorCode>> resets;
        std::vector<uint32_t> windows;
        std::string body;
        bool body_end = false;
        int goaways = 0;
        ErrorCode verdict = ErrorCode::no_error;

        ErrorCode on_request(uint32_t id, Block&& b, bool end, bool start) {
            requests.push_back({id, std::string(b.fields.get(":path").view()), end, start});
            return verdict;
        }

        void on_start(uint32_t id) {
            started.push_back(id);
        }

        void on_trailers(uint32_t id, Block&&) {
            trailers.push_back(id);
        }

        void on_data(uint32_t, const uint8_t* p, size_t n, bool end) {
            body.append(reinterpret_cast<const char*>(p), n);
            body_end = body_end || end;
        }

        void on_reset(uint32_t id, ErrorCode code) {
            resets.emplace_back(id, code);
        }

        void on_window(uint32_t id) {
            windows.push_back(id);
        }

        void on_goaway(uint32_t, ErrorCode) {
            ++goaways;
        }

        std::vector<std::string> pongs;

        void on_ping_ack(const uint8_t* opaque) {
            pongs.emplace_back(reinterpret_cast<const char*>(opaque), 8);
        }
    };

    // A frame the machine wrote
    struct Out {
        FrameType type;
        uint8_t flags;
        uint32_t stream;
        uint32_t code = 0;
        uint32_t last = 0;
        uint32_t increment = 0;
        std::string payload;
    };

    using Machine = ServerConnection<Sink>;

    struct Rig {
        Sink sink;
        Machine m;
        Encoder enc;
        std::string wire;         // the client's bytes not yet fed
        std::vector<Out> out;     // the machine's frames read so far
        int64_t now = 1;

        explicit Rig(const ServerSettings& s = ServerSettings(), bool preface = true)
        : m(sink, s) {
            m.start(now);
            if (preface) {
                wire.append(Preface, PrefaceSize);
                settings({});
            }
        }

        FrameWriter w() {
            return FrameWriter(wire);
        }

        void settings(std::vector<Setting> s) {
            w().settings(s.data(), s.size());
        }

        std::string block(const std::string& path, std::vector<std::pair<std::string, std::string>> extra = {}) {
            std::string b;
            enc.begin_block(b);
            enc.encode(b, ":method", "GET");
            enc.encode(b, ":scheme", "https");
            enc.encode(b, ":authority", "example.com");
            enc.encode(b, ":path", path);
            for (auto& [n, v] : extra) {
                enc.encode(b, n, v);
            }
            return b;
        }

        void request(uint32_t id, const std::string& path, bool end_stream = true) {
            const std::string b = block(path);
            w().headers(id, reinterpret_cast<const uint8_t*>(b.data()), b.size(), end_stream, true);
        }

        void data(uint32_t id, const std::string& s, bool end) {
            w().data(id, reinterpret_cast<const uint8_t*>(s.data()), s.size(), end);
        }

        // a body in frames of the default size
        void body(uint32_t id, const std::string& s, bool end) {
            for (size_t at = 0; at < s.size(); at += DefaultMaxFrameSize) {
                const size_t n = std::min<size_t>(DefaultMaxFrameSize, s.size() - at);
                w().data(id, reinterpret_cast<const uint8_t*>(s.data()) + at, n, end && at + n == s.size());
            }
        }

        // everything written so far fed at once; the machine's output read
        expected<size_t, Error> feed() {
            auto r = m.feed(reinterpret_cast<const uint8_t*>(wire.data()), wire.size(), now);
            if (r.has_value()) {
                wire.erase(0, *r);
            }
            drain();
            return r;
        }

        void drain() {
            auto o = m.output();
            const uint8_t* p = reinterpret_cast<const uint8_t*>(o.data());
            size_t at = 0;
            while (at < o.size()) {
                auto f = parse_frame(p + at, o.size() - at, LargestMaxFrameSize);
                ASSERT_TRUE(f.has_value());
                ASSERT_GT(f->size, 0u);
                const Frame& fr = f->frame;
                Out x{fr.type(), fr.header.flags, fr.header.stream, fr.error_code, fr.last_stream, fr.increment,
                      std::string(reinterpret_cast<const char*>(fr.payload.data()), fr.payload.size())};
                out.push_back(x);
                at += f->size;
            }
            m.written(o.size());
        }

        const Out* last(FrameType t) const {
            for (size_t i = out.size(); i-- > 0;) {
                if (out[i].type == t) {
                    return &out[i];
                }
            }
            return nullptr;
        }

        size_t count(FrameType t) const {
            size_t n = 0;
            for (auto& o : out) {
                n += o.type == t;
            }
            return n;
        }

        // the connection's error: GOAWAY with the code, feed refused
        void expect_goaway(ErrorCode code) {
            auto r = feed();
            ASSERT_FALSE(r.has_value());
            EXPECT_EQ(r.error().code, code) << r.error().what;
            const Out* g = last(FrameType::goaway);
            ASSERT_NE(g, nullptr);
            EXPECT_EQ(g->code, uint32_t(code));
            EXPECT_TRUE(m.failed());
        }

        // the stream's error: RST_STREAM with the code, the connection lives
        void expect_rst(uint32_t id, ErrorCode code) {
            auto r = feed();
            ASSERT_TRUE(r.has_value()) << r.error().what;
            const Out* rst = last(FrameType::rst_stream);
            ASSERT_NE(rst, nullptr);
            EXPECT_EQ(rst->stream, id);
            EXPECT_EQ(rst->code, uint32_t(code));
            EXPECT_EQ(last(FrameType::goaway), nullptr);
        }
    };

    std::string bytes(size_t n, char c = 'x') {
        return std::string(n, c);
    }
}

TEST(H2Connection_Tests, Preface_3_4) {
    Rig r(ServerSettings(), false);
    // the server's own preface: SETTINGS, then the connection's window
    r.drain();
    ASSERT_GE(r.out.size(), 2u);
    EXPECT_EQ(r.out[0].type, FrameType::settings);
    EXPECT_EQ(r.out[1].type, FrameType::window_update);
    EXPECT_EQ(r.out[1].increment, (1u << 20) - DefaultWindow);
    // the client's, a byte at a time: nothing taken until it is whole
    std::string p(Preface, PrefaceSize);
    for (size_t n = 0; n < PrefaceSize; ++n) {
        auto f = r.m.feed(reinterpret_cast<const uint8_t*>(p.data()), n, 1);
        ASSERT_TRUE(f.has_value());
        EXPECT_EQ(*f, 0u);
    }
    Rig bad(ServerSettings(), false);
    bad.wire = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
    bad.expect_goaway(ErrorCode::protocol_error);
}

TEST(H2Connection_Tests, FirstFrameIsSettings_3_4) {
    Rig r(ServerSettings(), false);
    r.wire.append(Preface, PrefaceSize);
    const uint8_t ping[8] = {};
    r.w().ping(ping, false);
    r.expect_goaway(ErrorCode::protocol_error);
}

TEST(H2Connection_Tests, Settings_6_5) {
    Rig r;
    r.settings({{uint16_t(SettingId::max_frame_size), 1 << 15}, {uint16_t(SettingId::header_table_size), 100}});
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.count(FrameType::settings), 3u);   // ours, and an ACK for each of the client's two
    const Out* ack = r.last(FrameType::settings);
    EXPECT_EQ(ack->flags, flag::ack);
    EXPECT_EQ(r.m.peer().max_frame_size, 1u << 15);
    EXPECT_EQ(r.m.peer().header_table_size, 100u);
    // an ACK of ours; then one for nothing
    r.w().settings_ack();
    ASSERT_TRUE(r.feed().has_value());
    r.w().settings_ack();
    r.expect_goaway(ErrorCode::protocol_error);
}

TEST(H2Connection_Tests, SettingsTimeout_6_5_3) {
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_TRUE(r.m.tick(r.now + 9'000'000'000).has_value());
    auto t = r.m.tick(r.now + 11'000'000'000);
    ASSERT_FALSE(t.has_value());
    EXPECT_EQ(t.error().code, ErrorCode::settings_timeout);
    // acknowledged in time: no timeout
    Rig ok;
    ok.w().settings_ack();
    ASSERT_TRUE(ok.feed().has_value());
    EXPECT_TRUE(ok.m.tick(ok.now + 60'000'000'000).has_value());
}

TEST(H2Connection_Tests, Request_8_1) {
    Rig r;
    r.request(1, "/hello");
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.requests.size(), 1u);
    EXPECT_EQ(r.sink.requests[0].id, 1u);
    EXPECT_EQ(r.sink.requests[0].path, "/hello");
    EXPECT_TRUE(r.sink.requests[0].end);
    EXPECT_TRUE(r.sink.requests[0].start);
    EXPECT_EQ(r.m.open_streams(), 1u);   // half-closed (remote)
    // the answer: HEADERS, then DATA with END_STREAM; the stream closes
    const uint8_t status200[] = {0x88};
    EXPECT_TRUE(r.m.send_headers(1, status200, 1, false));
    const std::string body = "hi";
    EXPECT_EQ(r.m.send_data(1, reinterpret_cast<const uint8_t*>(body.data()), 2, true), 2u);
    r.drain();
    EXPECT_EQ(r.m.open_streams(), 0u);
    const Out* h = r.last(FrameType::headers);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->payload, "\x88");
    EXPECT_EQ(h->flags, flag::end_headers);
    const Out* d = r.last(FrameType::data);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->payload, "hi");
    EXPECT_EQ(d->flags, flag::end_stream);
    // nothing more may be sent on it
    EXPECT_FALSE(r.m.send_headers(1, status200, 1, true));
    r.m.release(1);
    EXPECT_EQ(r.m.handlers(), 0u);
}

TEST(H2Connection_Tests, RequestBody_6_1) {
    Rig r;
    r.request(1, "/upload", false);
    r.data(1, "abc", false);
    // padded: the padding is given back at once
    const std::string pad = std::string("\x04", 1) + "def" + std::string(4, '\0');
    r.wire += std::string{0, 0, char(pad.size()), 0, char(flag::padded | flag::end_stream), 0, 0, 0, 1} + pad;
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.sink.body, "abcdef");
    EXPECT_TRUE(r.sink.body_end);
    // consumed: WINDOW_UPDATE once half a window has gathered
    r.m.consumed(1, 6);
    r.drain();
    const size_t updates = r.count(FrameType::window_update);
    r.m.consumed(1, (1u << 19));
    r.drain();
    EXPECT_GT(r.count(FrameType::window_update), updates);
    EXPECT_EQ(r.last(FrameType::window_update)->stream, 0u);   // the stream ended: only the connection's
}

TEST(H2Connection_Tests, StreamWindowUpdate_6_9) {
    Rig r;
    r.request(1, "/upload", false);
    r.data(1, bytes(1000), false);
    ASSERT_TRUE(r.feed().has_value());
    r.m.consumed(1, 1000);
    r.drain();
    EXPECT_EQ(r.count(FrameType::window_update), 1u);   // our preface's only: not half a window yet
    r.data(1, bytes(16000), false);
    for (int i = 0; i < 40; ++i) {
        r.data(1, bytes(16000), false);
    }
    ASSERT_TRUE(r.feed().has_value());
    r.m.consumed(1, 41 * 16000);
    r.drain();
    bool stream = false, connection = false;
    for (auto& o : r.out) {
        if (o.type == FrameType::window_update && o.stream == 1) {
            stream = true;
        }
        if (o.type == FrameType::window_update && o.stream == 0 && o.increment < (1u << 20) - DefaultWindow) {
            connection = true;
        }
    }
    EXPECT_TRUE(stream);
    EXPECT_TRUE(connection);
}

TEST(H2Connection_Tests, Continuation_6_10) {
    Rig r;
    const std::string b = r.block("/joined", {{"x-long", bytes(100, 'v')}});
    r.w().headers(1, reinterpret_cast<const uint8_t*>(b.data()), 10, true, false);
    r.w().continuation(1, reinterpret_cast<const uint8_t*>(b.data()) + 10, 20, false);
    r.w().continuation(1, reinterpret_cast<const uint8_t*>(b.data()) + 30, b.size() - 30, true);
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.requests.size(), 1u);
    EXPECT_EQ(r.sink.requests[0].path, "/joined");
    // a frame cutting a block
    Rig cut;
    const std::string c = cut.block("/x");
    cut.w().headers(1, reinterpret_cast<const uint8_t*>(c.data()), 2, true, false);
    const uint8_t ping[8] = {};
    cut.w().ping(ping, false);
    cut.expect_goaway(ErrorCode::protocol_error);
    // CONTINUATION on another stream
    Rig other;
    const std::string o = other.block("/x");
    other.w().headers(1, reinterpret_cast<const uint8_t*>(o.data()), 2, true, false);
    other.w().continuation(3, reinterpret_cast<const uint8_t*>(o.data()) + 2, o.size() - 2, true);
    other.expect_goaway(ErrorCode::protocol_error);
    // CONTINUATION with nothing before it
    Rig alone;
    alone.w().continuation(1, reinterpret_cast<const uint8_t*>(o.data()), o.size(), true);
    alone.expect_goaway(ErrorCode::protocol_error);
}

TEST(H2Connection_Tests, StreamIdentifiers_5_1_1) {
    Rig even;
    even.request(2, "/");
    even.expect_goaway(ErrorCode::protocol_error);
    // a smaller identifier than one seen: a closed stream
    Rig down;
    down.request(5, "/");
    down.request(3, "/");
    down.expect_goaway(ErrorCode::stream_closed);
    EXPECT_EQ(down.sink.requests.size(), 1u);
}

TEST(H2Connection_Tests, Idle_5_1) {
    Rig data;
    data.data(1, "x", false);
    data.expect_goaway(ErrorCode::protocol_error);
    Rig window;
    window.w().window_update(1, 10);
    window.expect_goaway(ErrorCode::protocol_error);
    Rig rst;
    rst.w().rst_stream(1, ErrorCode::cancel);
    rst.expect_goaway(ErrorCode::protocol_error);
    // an increment of 0 is a stream's error, but on an idle stream the
    // connection's
    Rig zero;
    zero.w().window_update(1, 0);
    zero.expect_goaway(ErrorCode::protocol_error);
    Rig open;
    open.request(1, "/", false);
    open.w().window_update(1, 0);
    open.expect_rst(1, ErrorCode::protocol_error);
    // PRIORITY on an idle stream is allowed and opens nothing
    Rig priority;
    priority.w().priority(3, Priority{false, 0, 15});
    priority.request(1, "/");
    ASSERT_TRUE(priority.feed().has_value());
    EXPECT_EQ(priority.sink.requests.size(), 1u);
}

TEST(H2Connection_Tests, HalfClosedRemote_5_1) {
    Rig r;
    r.request(1, "/");
    r.data(1, "late", false);
    r.expect_rst(1, ErrorCode::stream_closed);
    EXPECT_EQ(r.sink.resets.size(), 1u);
}

TEST(H2Connection_Tests, Closed_5_1) {
    // reset by us: late frames are ignored, no second RST
    Rig r;
    r.request(1, "/", false);
    ASSERT_TRUE(r.feed().has_value());
    r.m.reset(1, ErrorCode::cancel);
    r.drain();
    EXPECT_EQ(r.count(FrameType::rst_stream), 1u);
    r.data(1, "in flight", true);
    r.w().window_update(1, 5);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.count(FrameType::rst_stream), 1u);
    EXPECT_EQ(r.sink.body, "");
    // reset by the peer: the server is told; a DATA after it is a closed stream
    Rig p;
    p.request(1, "/", false);
    p.w().rst_stream(1, ErrorCode::cancel);
    ASSERT_TRUE(p.feed().has_value());
    ASSERT_EQ(p.sink.resets.size(), 1u);
    EXPECT_EQ(p.sink.resets[0].second, ErrorCode::cancel);
    p.data(1, "x", false);
    p.expect_rst(1, ErrorCode::stream_closed);
}

TEST(H2Connection_Tests, MaxConcurrentStreams_5_1_2) {
    ServerSettings s;
    s.max_concurrent_streams = 2;
    Rig r(s);
    r.request(1, "/a", false);
    r.request(3, "/b", false);
    r.request(5, "/c", false);
    r.expect_rst(5, ErrorCode::refused_stream);
    EXPECT_EQ(r.sink.requests.size(), 2u);
    // the refused stream's block was decoded all the same: the next block,
    // which indexes the table the refused one filled, reads right
    r.data(1, "", true);
    ASSERT_TRUE(r.feed().has_value());
    const uint8_t status[] = {0x88};
    r.m.send_headers(1, status, 1, true);
    r.request(7, "/c");
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.requests.size(), 3u);
    EXPECT_EQ(r.sink.requests[2].path, "/c");
}

TEST(H2Connection_Tests, Refused_8_1_1) {
    // a request the server finds malformed: RST_STREAM with its code
    Rig r;
    r.sink.verdict = ErrorCode::protocol_error;
    r.request(1, "/");
    r.expect_rst(1, ErrorCode::protocol_error);
    EXPECT_EQ(r.m.open_streams(), 0u);
    EXPECT_EQ(r.m.handlers(), 0u);
}

TEST(H2Connection_Tests, ReceiveWindow_6_9_1) {
    ServerSettings s;
    s.initial_window = DefaultWindow;
    s.connection_window = 1 << 20;
    Rig r(s);
    r.request(1, "/", false);
    r.body(1, bytes(DefaultWindow), false);
    ASSERT_TRUE(r.feed().has_value());
    r.data(1, "!", false);
    r.expect_rst(1, ErrorCode::flow_control_error);
    // the connection's window
    ServerSettings c;
    c.initial_window = LargestWindow;
    c.connection_window = DefaultWindow;
    Rig k(c);
    k.request(1, "/", false);
    k.body(1, bytes(DefaultWindow), false);
    ASSERT_TRUE(k.feed().has_value());
    k.data(1, "!", false);
    k.expect_goaway(ErrorCode::flow_control_error);
}

TEST(H2Connection_Tests, SendWindow_6_9) {
    Rig r;
    r.settings({{uint16_t(SettingId::initial_window_size), 10}});
    r.request(1, "/", true);
    ASSERT_TRUE(r.feed().has_value());
    const std::string body = bytes(25);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(body.data());
    EXPECT_EQ(r.m.send_data(1, p, 25, true), 10u);
    EXPECT_EQ(r.m.stream_send_window(1), 0);
    // the peer's WINDOW_UPDATE opens it
    r.w().window_update(1, 5);
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_FALSE(r.sink.windows.empty());
    EXPECT_EQ(r.sink.windows.back(), 1u);
    EXPECT_EQ(r.m.send_data(1, p + 10, 15, true), 5u);
    // a larger SETTINGS_INITIAL_WINDOW_SIZE moves the open stream's window
    r.settings({{uint16_t(SettingId::initial_window_size), 100}});
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.m.stream_send_window(1), 90);
    EXPECT_EQ(r.m.send_data(1, p + 15, 10, true), 10u);
    r.drain();
    size_t sent = 0;
    for (auto& o : r.out) {
        if (o.type == FrameType::data) {
            sent += o.payload.size();
        }
    }
    EXPECT_EQ(sent, 25u);
    EXPECT_EQ(r.last(FrameType::data)->flags, flag::end_stream);
}

TEST(H2Connection_Tests, WindowOverflow_6_9_1) {
    Rig r;
    r.w().window_update(0, LargestWindow);
    r.expect_goaway(ErrorCode::flow_control_error);
    Rig s;
    s.request(1, "/");
    s.w().window_update(1, LargestWindow);
    s.expect_rst(1, ErrorCode::flow_control_error);
    // through SETTINGS: the connection's error (§6.9.2)
    Rig t;
    t.request(1, "/");
    ASSERT_TRUE(t.feed().has_value());
    t.w().window_update(1, LargestWindow - DefaultWindow);
    t.settings({{uint16_t(SettingId::initial_window_size), DefaultWindow + 1}});
    t.expect_goaway(ErrorCode::flow_control_error);
}

TEST(H2Connection_Tests, PeerFrameSize_4_2) {
    Rig r;
    r.request(1, "/");
    ASSERT_TRUE(r.feed().has_value());
    r.w().window_update(0, 1 << 20);
    r.w().window_update(1, 1 << 20);
    ASSERT_TRUE(r.feed().has_value());
    const std::string body = bytes(40000);
    EXPECT_EQ(r.m.send_data(1, reinterpret_cast<const uint8_t*>(body.data()), body.size(), false), body.size());
    const std::string block = bytes(20000, '\x40');
    EXPECT_TRUE(r.m.send_headers(1, reinterpret_cast<const uint8_t*>(block.data()), block.size(), true));
    r.drain();
    size_t data = 0;
    for (auto& o : r.out) {
        EXPECT_LE(o.payload.size(), DefaultMaxFrameSize);
        data += o.type == FrameType::data;
    }
    EXPECT_EQ(data, 3u);
    EXPECT_EQ(r.count(FrameType::continuation), 1u);
    EXPECT_EQ(r.last(FrameType::continuation)->flags, flag::end_headers);
}

TEST(H2Connection_Tests, Trailers_8_1) {
    Rig r;
    r.request(1, "/", false);
    r.data(1, "body", false);
    std::string t;
    r.enc.encode(t, "x-checksum", "abc");
    r.w().headers(1, reinterpret_cast<const uint8_t*>(t.data()), t.size(), true, true);
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.trailers.size(), 1u);
    // trailers without END_STREAM
    Rig bad;
    bad.request(1, "/", false);
    std::string u;
    bad.enc.encode(u, "x-checksum", "abc");
    bad.w().headers(1, reinterpret_cast<const uint8_t*>(u.data()), u.size(), false, true);
    bad.expect_rst(1, ErrorCode::protocol_error);
}

TEST(H2Connection_Tests, SelfDependency_5_3_1) {
    Rig r;
    const std::string b = r.block("/self", {{"x-a", "1"}});
    Priority p{false, 1, 15};
    r.w().headers(1, reinterpret_cast<const uint8_t*>(b.data()), b.size(), true, true, &p);
    r.expect_rst(1, ErrorCode::protocol_error);
    EXPECT_TRUE(r.sink.requests.empty());
    // its block was decoded: the next, indexing what it added, reads right
    r.request(3, "/self");
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.requests.size(), 1u);
    EXPECT_EQ(r.sink.requests[0].path, "/self");
    // PRIORITY depending on itself, and of a wrong length
    r.w().priority(5, Priority{false, 5, 1});
    r.expect_rst(5, ErrorCode::protocol_error);
    r.wire += std::string{0, 0, 4, 2, 0, 0, 0, 0, 7} + std::string(4, '\0');
    r.expect_rst(7, ErrorCode::frame_size_error);
}

TEST(H2Connection_Tests, PushPromise_8_4) {
    Rig r;
    r.request(1, "/");
    const uint8_t b[] = {0x82};
    r.w().push_promise(1, 2, b, 1, true);
    r.expect_goaway(ErrorCode::protocol_error);
}

TEST(H2Connection_Tests, Ping_6_7) {
    Rig r;
    const uint8_t opaque[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    r.w().ping(opaque, false);
    ASSERT_TRUE(r.feed().has_value());
    const Out* p = r.last(FrameType::ping);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->flags, flag::ack);
    EXPECT_EQ(p->payload, std::string("\1\2\3\4\5\6\7\x08"));
    // an ACK is not answered; it is our ping()'s answer
    r.w().ping(opaque, true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.count(FrameType::ping), 1u);
    ASSERT_EQ(r.sink.pongs.size(), 1u);
    EXPECT_EQ(r.sink.pongs[0], std::string("\1\2\3\4\5\6\7\x08"));
    r.m.ping(opaque);
    r.drain();
    EXPECT_EQ(r.count(FrameType::ping), 2u);
    EXPECT_EQ(r.last(FrameType::ping)->flags, 0);
}

TEST(H2Connection_Tests, PeerTableSize_6_5_2) {
    // the encoder's table follows the peer's SETTINGS_HEADER_TABLE_SIZE
    Rig r;
    r.settings({{uint16_t(SettingId::header_table_size), 100}});
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.m.encoder().table().limit(), 100u);
    // the next block starts with a Dynamic Table Size Update (RFC 7541 §4.2)
    std::string b;
    r.m.encoder().begin_block(b);
    ASSERT_FALSE(b.empty());
    EXPECT_EQ(uint8_t(b[0]) & 0xE0, 0x20);
}

TEST(H2Connection_Tests, ResetGivesWindowBack_6_9) {
    // bytes a stream received and its owner never took go back to the
    // connection's window when it is reset
    ServerSettings s;
    s.connection_window = DefaultWindow;
    Rig r(s);
    r.request(1, "/", false);
    r.body(1, bytes(40000), false);
    ASSERT_TRUE(r.feed().has_value());
    const size_t before = r.count(FrameType::window_update);
    r.m.reset(1, ErrorCode::cancel);
    r.drain();
    ASSERT_EQ(r.count(FrameType::window_update), before + 1);
    EXPECT_EQ(r.last(FrameType::window_update)->stream, 0u);
    EXPECT_EQ(r.last(FrameType::window_update)->increment, 40000u);
    // the same when the peer resets it
    Rig p(s);
    p.request(1, "/", false);
    p.body(1, bytes(40000), false);
    p.w().rst_stream(1, ErrorCode::cancel);
    ASSERT_TRUE(p.feed().has_value());
    ASSERT_NE(p.last(FrameType::window_update), nullptr);
    EXPECT_EQ(p.last(FrameType::window_update)->increment, 40000u);
}

TEST(H2Connection_Tests, Goaway_6_8) {
    Rig r;
    r.request(1, "/", false);
    ASSERT_TRUE(r.feed().has_value());
    r.m.drain();
    r.drain();
    const Out* g = r.last(FrameType::goaway);
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->last, 0x7FFFFFFFu);
    const Out* ping = r.last(FrameType::ping);
    ASSERT_NE(ping, nullptr);
    // a stream opened before the PING comes back is still served
    r.request(3, "/", true);
    r.w().ping(reinterpret_cast<const uint8_t*>(ping->payload.data()), true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.sink.requests.size(), 2u);
    g = r.last(FrameType::goaway);
    EXPECT_EQ(g->last, 3u);
    EXPECT_EQ(g->code, uint32_t(ErrorCode::no_error));
    // after it: not served, the block still decoded
    r.request(5, "/");
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.sink.requests.size(), 2u);
    EXPECT_FALSE(r.m.finished());
    const uint8_t status[] = {0x88};
    r.m.send_headers(3, status, 1, true);
    r.m.reset(1, ErrorCode::no_error);
    EXPECT_TRUE(r.m.finished());
    // the peer's GOAWAY is told to the server
    Rig p;
    p.w().goaway(0, ErrorCode::no_error);
    ASSERT_TRUE(p.feed().has_value());
    EXPECT_EQ(p.sink.goaways, 1);
}

// --- the attacks ---------------------------------------------------------------

TEST(H2Connection_Tests, Attack_HpackBomb_431) {
    // a list past max_header_list_size in a block under the bytes' limit:
    // 431 on the stream, the connection lives
    ServerSettings s;
    s.max_header_list_size = 1024;
    Rig r(s);
    const std::string b = r.block("/bomb", {{"x-big", bytes(2000)}});
    ASSERT_LT(b.size(), 64u * 1024);
    r.w().headers(1, reinterpret_cast<const uint8_t*>(b.data()), b.size(), false, true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_TRUE(r.sink.requests.empty());
    const Out* h = r.last(FrameType::headers);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->stream, 1u);
    EXPECT_EQ(h->payload, std::string("\x08\x03" "431", 5));
    EXPECT_EQ(h->flags, flag::end_stream | flag::end_headers);
    const Out* rst = r.last(FrameType::rst_stream);
    ASSERT_NE(rst, nullptr);
    EXPECT_EQ(rst->code, uint32_t(ErrorCode::no_error));
    // its body in flight is ignored; the next request is served
    r.data(1, "rest", true);
    r.request(3, "/next");
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.requests.size(), 1u);
    EXPECT_EQ(r.sink.requests[0].path, "/next");
    EXPECT_EQ(r.count(FrameType::rst_stream), 1u);
}

TEST(H2Connection_Tests, Attack_ContinuationFlood) {
    // CONTINUATIONs without end: past max(2 × list, 64 KB) the connection
    // ends before anything is decoded
    Rig r;
    const std::string b = r.block("/flood");
    r.w().headers(1, reinterpret_cast<const uint8_t*>(b.data()), b.size(), true, false);
    const std::string chunk = bytes(16000, '\x40');
    for (int i = 0; i < 4; ++i) {
        r.w().continuation(1, reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size(), false);
    }
    ASSERT_TRUE(r.feed().has_value());   // 64 000 bytes and a little: still under
    r.w().continuation(1, reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size(), false);
    r.expect_goaway(ErrorCode::enhance_your_calm);
    EXPECT_TRUE(r.sink.requests.empty());
}

TEST(H2Connection_Tests, Attack_FirstFragment) {
    // with a large SETTINGS_MAX_FRAME_SIZE, one HEADERS alone past the
    // bytes' limit: GOAWAY before anything is gathered or decoded
    ServerSettings s;
    s.max_frame_size = 1 << 20;
    Rig r(s);
    const std::string big = bytes(100 * 1024, '\x40');
    r.w().headers(1, reinterpret_cast<const uint8_t*>(big.data()), big.size(), true, false);
    r.expect_goaway(ErrorCode::enhance_your_calm);
    Rig whole(s);
    whole.w().headers(1, reinterpret_cast<const uint8_t*>(big.data()), big.size(), true, true);
    whole.expect_goaway(ErrorCode::enhance_your_calm);
    EXPECT_TRUE(whole.sink.requests.empty());
}

TEST(H2Connection_Tests, Attack_ResetRate) {
    // nghttp2's bucket beside Go's queue: RST_STREAM received past
    // max_resets, refilled at resets_per_second
    ServerSettings s;
    s.max_resets = 10;
    s.resets_per_second = 2;
    Rig r(s);
    uint32_t id = 1;
    auto open_and_reset = [&](int n) {
        for (int i = 0; i < n; ++i, id += 2) {
            r.request(id, "/", true);
            r.w().rst_stream(id, ErrorCode::cancel);
        }
    };
    open_and_reset(10);
    ASSERT_TRUE(r.feed().has_value());
    r.now += 1'000'000'000;   // a second: two more
    open_and_reset(2);
    ASSERT_TRUE(r.feed().has_value());
    open_and_reset(1);
    r.expect_goaway(ErrorCode::enhance_your_calm);
    // the defaults are nghttp2's
    ServerSettings d;
    EXPECT_EQ(d.max_resets, 1000u);
    EXPECT_EQ(d.resets_per_second, 33u);
}

TEST(H2Connection_Tests, Attack_RapidReset) {
    // handlers that do not end at once; streams opened and reset one after
    // another: past max_concurrent_streams they wait, past 4 × that waiting
    // the connection ends (Go's numbers)
    ServerSettings s;
    s.max_concurrent_streams = 4;
    Rig r(s);
    uint32_t id = 1;
    for (int i = 0; i < 4; ++i, id += 2) {
        r.request(id, "/", true);
        r.w().rst_stream(id, ErrorCode::cancel);
    }
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.m.handlers(), 4u);
    EXPECT_EQ(r.m.open_streams(), 0u);
    EXPECT_EQ(r.sink.resets.size(), 4u);
    for (int i = 0; i < 17; ++i, id += 2) {
        r.request(id, "/", true);
        r.w().rst_stream(id, ErrorCode::cancel);
    }
    ASSERT_TRUE(r.feed().has_value());   // 17 waiting: 4 × 4 + 1, the most Go keeps
    r.request(id, "/", true);
    r.expect_goaway(ErrorCode::enhance_your_calm);
    // a handler ending passes its place to a live request, skipping the reset
    Rig q(s);
    for (uint32_t k = 1; k <= 7; k += 2) {
        q.request(k, "/", true);
        q.w().rst_stream(k, ErrorCode::cancel);
    }
    q.request(9, "/", false);
    q.request(11, "/", false);
    q.w().rst_stream(9, ErrorCode::cancel);
    ASSERT_TRUE(q.feed().has_value());
    EXPECT_EQ(q.m.handlers(), 4u);
    EXPECT_FALSE(q.sink.requests[4].start);
    EXPECT_FALSE(q.sink.requests[5].start);
    q.m.release(1);
    ASSERT_EQ(q.sink.started.size(), 1u);
    EXPECT_EQ(q.sink.started[0], 11u);
}

TEST(H2Connection_Tests, Attack_ControlFlood) {
    // PINGs whose answers the peer never reads
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    const uint8_t opaque[8] = {};
    std::string many;
    for (int i = 0; i < 10001; ++i) {
        FrameWriter(many).ping(opaque, false);
    }
    auto f = r.m.feed(reinterpret_cast<const uint8_t*>(many.data()), many.size(), r.now);
    ASSERT_FALSE(f.has_value());
    EXPECT_EQ(f.error().code, ErrorCode::enhance_your_calm);
    // read as they come: no limit reached
    Rig ok;
    ASSERT_TRUE(ok.feed().has_value());
    for (int i = 0; i < 20; ++i) {
        std::string some;
        for (int j = 0; j < 1000; ++j) {
            FrameWriter(some).ping(opaque, false);
        }
        auto g = ok.m.feed(reinterpret_cast<const uint8_t*>(some.data()), some.size(), ok.now);
        ASSERT_TRUE(g.has_value());
        ok.m.written(ok.m.output().size());
    }
    // SETTINGS the same
    Rig s;
    ASSERT_TRUE(s.feed().has_value());
    std::string settings;
    for (int i = 0; i < 10001; ++i) {
        FrameWriter(settings).settings(nullptr, 0);
    }
    auto h = s.m.feed(reinterpret_cast<const uint8_t*>(settings.data()), settings.size(), s.now);
    ASSERT_FALSE(h.has_value());
    EXPECT_EQ(h.error().code, ErrorCode::enhance_your_calm);
}

TEST(H2Connection_Tests, Attack_EmptyFrames) {
    Rig r;
    r.request(1, "/", false);
    for (int i = 0; i < 1000; ++i) {
        r.data(1, "", false);
    }
    ASSERT_TRUE(r.feed().has_value());
    r.data(1, "", false);
    r.expect_goaway(ErrorCode::enhance_your_calm);
    // with something between them: allowed
    Rig ok;
    ok.request(1, "/", false);
    for (int i = 0; i < 3000; ++i) {
        ok.data(1, "", false);
        if (i % 500 == 0) {
            ok.data(1, "x", false);
        }
    }
    ASSERT_TRUE(ok.feed().has_value());
    // PRIORITY and unknown frames count too
    Rig p;
    for (int i = 0; i < 1001; ++i) {
        p.w().priority(3, Priority{false, 0, 1});
    }
    p.expect_goaway(ErrorCode::enhance_your_calm);
}

TEST(H2Connection_Tests, Attack_WindowZero) {
    // a peer that never opens its window: the waiting stream is reset
    Rig r;
    r.settings({{uint16_t(SettingId::initial_window_size), 0}});
    r.w().settings_ack();
    r.request(1, "/");
    ASSERT_TRUE(r.feed().has_value());
    const std::string body = bytes(100);
    EXPECT_EQ(r.m.send_data(1, reinterpret_cast<const uint8_t*>(body.data()), body.size(), true), 0u);
    EXPECT_TRUE(r.m.tick(r.now + 29'000'000'000).has_value());
    EXPECT_TRUE(r.sink.resets.empty());
    EXPECT_TRUE(r.m.tick(r.now + 31'000'000'000).has_value());
    ASSERT_EQ(r.sink.resets.size(), 1u);
    EXPECT_EQ(r.sink.resets[0].second, ErrorCode::cancel);
    r.drain();
    EXPECT_EQ(r.last(FrameType::rst_stream)->code, uint32_t(ErrorCode::cancel));
    EXPECT_EQ(r.m.open_streams(), 0u);
}
