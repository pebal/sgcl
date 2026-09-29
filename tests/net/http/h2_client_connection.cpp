//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http/2: the client's connection machine, pure (sgcl/net/http/detail/h2/
// client_connection.h), fed frames written by a server made here
// (FrameWriter and HPACK's Encoder) and read back from its output. One test
// per rule of RFC 9113 the client's role keeps, named by the section: its
// preface, the identifiers it gives, the server's limit of concurrent
// streams, a response as 1xx, final fields, DATA and trailers (§8.1), no
// push (§8.4), idle streams, GOAWAY and the streams it leaves unprocessed
// (§6.8), REFUSED_STREAM (§8.7), flow control both ways with the client's
// windows (a stream nobody reads stops at its window, the others go on),
// the server's table size reaching the encoder, PING. What both roles
// share is tested with the server's (h2_connection.cpp).
#include "tests/types.h"
#include "sgcl/net/http/detail/h2/client_connection.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace sgcl;
using namespace sgcl::net::http::detail::h2;
using sgcl::net::http::detail::HeadersAccess;

namespace {
    // What the machine told the transport (std types: nothing managed kept
    // past a call)
    struct Sink {
        struct Response {
            uint32_t id;
            std::string status;
            bool end;
            bool informational;
            bool truncated;
        };
        std::vector<Response> responses;
        std::vector<std::pair<uint32_t, std::string>> trailers;   // the first trailer's "name: value"
        std::vector<std::pair<uint32_t, ErrorCode>> resets;
        std::vector<uint32_t> unprocessed;
        std::vector<uint32_t> windows;
        std::vector<std::pair<uint32_t, ErrorCode>> goaways;
        std::vector<std::string> pongs;
        std::string body;
        bool body_end = false;
        ErrorCode verdict = ErrorCode::no_error;

        ErrorCode on_response(uint32_t id, Block&& b, bool end, bool informational) {
            responses.push_back({id, std::string(b.fields.get(":status").view()), end, informational, b.truncated});
            return verdict;
        }

        void on_trailers(uint32_t id, Block&& b) {
            auto& f = HeadersAccess::fields(b.fields);
            trailers.emplace_back(id, f.empty() ? std::string() : std::string(f[0].first.view()) + ": " + std::string(f[0].second.view()));
        }

        void on_data(uint32_t, const uint8_t* p, size_t n, bool end) {
            body.append(reinterpret_cast<const char*>(p), n);
            body_end = body_end || end;
        }

        void on_reset(uint32_t id, ErrorCode code) {
            resets.emplace_back(id, code);
        }

        void on_unprocessed(uint32_t id) {
            unprocessed.push_back(id);
        }

        void on_window(uint32_t id) {
            windows.push_back(id);
        }

        void on_goaway(uint32_t last, ErrorCode code) {
            goaways.emplace_back(last, code);
        }

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
        std::vector<Setting> settings;
        std::vector<std::pair<std::string, std::string>> fields;   // HEADERS: the block as the server decodes it
    };

    using Machine = ClientConnection<Sink>;

    struct Rig {
        Sink sink;
        Machine m;
        Encoder enc;              // the server's
        Decoder dec;              // the server's, for the client's blocks
        std::string wire;         // the server's bytes not yet fed
        std::vector<Out> out;     // the machine's frames read so far
        std::string preface;      // the machine's first 24 bytes
        int64_t now = 1;

        // server_settings: the server's SETTINGS written first (as §3.4 asks)
        explicit Rig(const ClientSettings& s = ClientSettings(), bool server_settings = true, std::vector<Setting> first = {})
        : m(sink, s) {
            m.start(now);
            if (server_settings) {
                settings(first);
            }
        }

        FrameWriter w() {
            return FrameWriter(wire);
        }

        void settings(std::vector<Setting> s) {
            w().settings(s.data(), s.size());
        }

        // A request: encoded with the machine's encoder, then opened
        uint32_t open(const std::string& path, bool end_stream = true) {
            if (!m.can_open()) {
                return 0;
            }
            std::string b;
            auto& e = m.encoder();
            e.begin_block(b);
            e.encode(b, ":method", end_stream ? "GET" : "POST");
            e.encode(b, ":scheme", "https");
            e.encode(b, ":authority", "example.com");
            e.encode(b, ":path", path);
            return m.open_stream(reinterpret_cast<const uint8_t*>(b.data()), b.size(), end_stream);
        }

        void response(uint32_t id, const std::string& status, bool end_stream, std::vector<std::pair<std::string, std::string>> extra = {}) {
            std::string b;
            enc.begin_block(b);
            if (!status.empty()) {
                enc.encode(b, ":status", status);
            }
            for (auto& [n, v] : extra) {
                enc.encode(b, n, v);
            }
            w().headers(id, reinterpret_cast<const uint8_t*>(b.data()), b.size(), end_stream, true);
        }

        void data(uint32_t id, const std::string& s, bool end) {
            for (size_t at = 0; at < s.size() || (at == 0 && s.empty()); at += DefaultMaxFrameSize) {
                const size_t n = std::min<size_t>(DefaultMaxFrameSize, s.size() - at);
                w().data(id, reinterpret_cast<const uint8_t*>(s.data()) + at, n, end && at + n == s.size());
                if (s.empty()) {
                    break;
                }
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
            if (preface.empty()) {
                ASSERT_GE(o.size(), PrefaceSize);
                preface.assign(reinterpret_cast<const char*>(p), PrefaceSize);
                at = PrefaceSize;
            }
            while (at < o.size()) {
                auto f = parse_frame(p + at, o.size() - at, LargestMaxFrameSize);
                ASSERT_TRUE(f.has_value());
                ASSERT_GT(f->size, 0u);
                const Frame& fr = f->frame;
                Out x{fr.type(), fr.header.flags, fr.header.stream, fr.error_code, fr.last_stream, fr.increment,
                      std::string(reinterpret_cast<const char*>(fr.payload.data()), fr.payload.size()), {}};
                if (fr.type() == FrameType::settings) {
                    for (size_t i = 0; i < fr.settings_count(); ++i) {
                        x.settings.push_back(fr.setting(i));
                    }
                }
                if (fr.type() == FrameType::headers) {
                    // each block decoded once, in order (the table is the connection's)
                    auto b = dec.decode(fr.payload, 1 << 20);
                    ASSERT_TRUE(b.has_value());
                    for (auto& f : HeadersAccess::fields(b->fields)) {
                        x.fields.emplace_back(std::string(f.first.view()), std::string(f.second.view()));
                    }
                }
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

        // the request's fields as the server decodes them, from the last
        // HEADERS written
        std::string request_field(const std::string& name) {
            const Out* h = last(FrameType::headers);
            if (!h) {
                return "?";
            }
            for (auto& [n, v] : h->fields) {
                if (n == name) {
                    return v;
                }
            }
            return "";
        }

        void expect_goaway(ErrorCode code) {
            auto r = feed();
            ASSERT_FALSE(r.has_value());
            EXPECT_EQ(r.error().code, code) << r.error().what;
            const Out* g = last(FrameType::goaway);
            ASSERT_NE(g, nullptr);
            EXPECT_EQ(g->code, uint32_t(code));
            EXPECT_EQ(g->last, 0u);   // the server opened no stream
            EXPECT_TRUE(m.failed());
        }

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

    uint32_t setting(const Out& o, SettingId id) {
        for (auto& s : o.settings) {
            if (s.id == uint16_t(id)) {
                return s.value;
            }
        }
        return 0xDEADBEEF;
    }
}

TEST(H2ClientConnection_Tests, Preface_3_4) {
    Rig r(ClientSettings(), false);
    r.drain();
    EXPECT_EQ(r.preface, std::string(Preface, PrefaceSize));
    ASSERT_GE(r.out.size(), 2u);
    ASSERT_EQ(r.out[0].type, FrameType::settings);
    EXPECT_EQ(r.out[0].settings[0].id, uint16_t(SettingId::enable_push));   // first: no push
    EXPECT_EQ(setting(r.out[0], SettingId::enable_push), 0u);
    EXPECT_EQ(setting(r.out[0], SettingId::initial_window_size), 1u << 20);
    EXPECT_EQ(setting(r.out[0], SettingId::max_header_list_size), 1u << 20);
    EXPECT_EQ(r.out[1].type, FrameType::window_update);
    EXPECT_EQ(r.out[1].stream, 0u);
    EXPECT_EQ(r.out[1].increment, (16u << 20) - DefaultWindow);
    // a request may go before the server's SETTINGS (§3.4)
    EXPECT_FALSE(r.m.peer_settings_received());
    EXPECT_EQ(r.open("/early"), 1u);
    r.settings({});
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_TRUE(r.m.peer_settings_received());
    EXPECT_EQ(r.last(FrameType::settings)->flags, flag::ack);
}

TEST(H2ClientConnection_Tests, FirstFrameIsSettings_3_4) {
    Rig r(ClientSettings(), false);
    const uint8_t ping[8] = {};
    r.w().ping(ping, false);
    r.expect_goaway(ErrorCode::protocol_error);
}

TEST(H2ClientConnection_Tests, StreamIdentifiers_5_1_1) {
    Rig r;
    EXPECT_EQ(r.open("/a"), 1u);
    r.drain();
    const Out* h = r.last(FrameType::headers);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->stream, 1u);
    EXPECT_EQ(h->flags, flag::end_stream | flag::end_headers);
    EXPECT_EQ(r.request_field(":path"), "/a");
    EXPECT_EQ(r.request_field(":method"), "GET");
    EXPECT_EQ(r.open("/b", false), 3u);
    r.drain();
    EXPECT_EQ(r.last(FrameType::headers)->flags, flag::end_headers);   // a body follows
    EXPECT_EQ(r.request_field(":path"), "/b");
    EXPECT_EQ(r.open("/c"), 5u);
    EXPECT_EQ(r.m.open_streams(), 3u);
}

TEST(H2ClientConnection_Tests, MaxConcurrentStreams_5_1_2) {
    // before the server's SETTINGS: initial_concurrent_streams
    ClientSettings s;
    s.initial_concurrent_streams = 3;
    Rig early(s, false);
    EXPECT_EQ(early.m.stream_limit(), 3u);
    EXPECT_NE(early.open("/1"), 0u);
    EXPECT_NE(early.open("/2"), 0u);
    EXPECT_NE(early.open("/3"), 0u);
    EXPECT_FALSE(early.m.can_open());
    EXPECT_EQ(early.m.open_stream(nullptr, 0, true), 0u);
    // the server's limit, once its SETTINGS came
    Rig r(ClientSettings(), true, {{uint16_t(SettingId::max_concurrent_streams), 2}});
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.m.stream_limit(), 2u);
    EXPECT_EQ(r.open("/1"), 1u);
    EXPECT_EQ(r.open("/2"), 3u);
    EXPECT_EQ(r.open("/3"), 0u);
    // one ends: room for the next
    r.response(1, "200", true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.m.open_streams(), 1u);
    EXPECT_EQ(r.open("/3"), 5u);
    // ours caps the server's
    ClientSettings mine;
    mine.max_concurrent_streams = 4;
    Rig capped(mine, true, {{uint16_t(SettingId::max_concurrent_streams), 100}});
    ASSERT_TRUE(capped.feed().has_value());
    EXPECT_EQ(capped.m.stream_limit(), 4u);
}

TEST(H2ClientConnection_Tests, Response_8_1) {
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    const uint32_t id = r.open("/x");
    r.response(id, "103", false, {{"link", "</a.css>; rel=preload"}});
    r.response(id, "100", false);
    r.response(id, "200", false, {{"content-type", "text/plain"}});
    r.data(id, "hello ", false);
    r.data(id, "world", false);
    r.response(id, "", true, {{"x-checksum", "abc"}});
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.responses.size(), 3u);
    EXPECT_EQ(r.sink.responses[0].status, "103");
    EXPECT_TRUE(r.sink.responses[0].informational);
    EXPECT_TRUE(r.sink.responses[1].informational);
    EXPECT_EQ(r.sink.responses[2].status, "200");
    EXPECT_FALSE(r.sink.responses[2].informational);
    EXPECT_FALSE(r.sink.responses[2].end);
    EXPECT_EQ(r.sink.body, "hello world");
    ASSERT_EQ(r.sink.trailers.size(), 1u);
    EXPECT_EQ(r.sink.trailers[0].second, "x-checksum: abc");
    EXPECT_EQ(r.m.open_streams(), 0u);
    EXPECT_TRUE(r.sink.resets.empty());
    // a response with END_STREAM on its fields
    const uint32_t id2 = r.open("/y");
    r.response(id2, "204", true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_TRUE(r.sink.responses.back().end);
    EXPECT_EQ(r.m.open_streams(), 0u);
}

TEST(H2ClientConnection_Tests, MalformedResponses_8_1) {
    // a 1xx with END_STREAM
    {
        Rig r;
        const uint32_t id = r.open("/");
        r.response(id, "100", true);
        r.expect_rst(id, ErrorCode::protocol_error);
        ASSERT_EQ(r.sink.resets.size(), 1u);
        EXPECT_EQ(r.sink.resets[0], std::make_pair(id, ErrorCode::protocol_error));
        EXPECT_TRUE(r.sink.responses.empty());
    }
    // 101: no Upgrade in HTTP/2 (§8.6)
    {
        Rig r;
        const uint32_t id = r.open("/");
        r.response(id, "101", false);
        r.expect_rst(id, ErrorCode::protocol_error);
    }
    // DATA before the final fields
    {
        Rig r;
        const uint32_t id = r.open("/");
        r.response(id, "100", false);
        r.data(id, "early", false);
        r.expect_rst(id, ErrorCode::protocol_error);
        EXPECT_TRUE(r.sink.body.empty());
    }
    // trailers without END_STREAM, or with a pseudo-field
    {
        Rig r;
        const uint32_t id = r.open("/");
        r.response(id, "200", false);
        r.response(id, "", false, {{"x-t", "1"}});
        r.expect_rst(id, ErrorCode::protocol_error);
    }
    {
        Rig r;
        const uint32_t id = r.open("/");
        r.response(id, "200", false);
        r.response(id, "200", true);
        r.expect_rst(id, ErrorCode::protocol_error);
        EXPECT_TRUE(r.sink.trailers.empty());
    }
    // the transport's verdict (here: no :status) resets with its code
    {
        Rig r;
        r.sink.verdict = ErrorCode::protocol_error;
        const uint32_t id = r.open("/");
        r.response(id, "", false, {{"content-type", "x"}});
        r.expect_rst(id, ErrorCode::protocol_error);
        EXPECT_EQ(r.m.open_streams(), 0u);
    }
    // a field block past our max_header_list_size comes truncated, the
    // transport's call
    {
        ClientSettings s;
        s.max_header_list_size = 100;
        Rig r(s);
        r.sink.verdict = ErrorCode::cancel;
        const uint32_t id = r.open("/");
        r.response(id, "200", false, {{"x-big", std::string(200, 'b')}});
        r.expect_rst(id, ErrorCode::cancel);
        ASSERT_EQ(r.sink.responses.size(), 1u);
        EXPECT_TRUE(r.sink.responses[0].truncated);
    }
}

TEST(H2ClientConnection_Tests, HalfClosedRemote_5_1) {
    // the request's body still going, the response ended: more HEADERS
    // from the server is STREAM_CLOSED on the stream
    Rig r;
    const uint32_t id = r.open("/upload", false);
    r.response(id, "413", true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.m.open_streams(), 1u);   // half-closed (remote)
    r.response(id, "200", false);
    r.expect_rst(id, ErrorCode::stream_closed);
    EXPECT_EQ(r.m.open_streams(), 0u);
}

TEST(H2ClientConnection_Tests, NoPush_8_4) {
    {
        Rig r;
        const uint32_t id = r.open("/");
        const uint8_t block[] = {0x82};
        r.w().push_promise(id, 2, block, 1, true);
        r.expect_goaway(ErrorCode::protocol_error);
    }
    // HEADERS on an even stream: the server may open none
    {
        Rig r;
        r.open("/");
        r.response(2, "200", true);
        r.expect_goaway(ErrorCode::protocol_error);
    }
}

TEST(H2ClientConnection_Tests, Idle_5_1) {
    // frames on a stream not yet opened
    {
        Rig r;
        r.open("/");
        r.data(3, "x", true);
        r.expect_goaway(ErrorCode::protocol_error);
    }
    {
        Rig r;
        r.w().rst_stream(1, ErrorCode::cancel);
        r.expect_goaway(ErrorCode::protocol_error);
    }
    {
        Rig r;
        r.response(1, "200", true);
        r.expect_goaway(ErrorCode::protocol_error);
    }
    {
        Rig r;
        r.open("/");
        r.w().window_update(4, 100);
        r.expect_goaway(ErrorCode::protocol_error);
    }
}

TEST(H2ClientConnection_Tests, Goaway_6_8) {
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    const uint32_t a = r.open("/a");
    const uint32_t b = r.open("/b");
    const uint32_t c = r.open("/c");
    r.w().goaway(a, ErrorCode::no_error);
    ASSERT_TRUE(r.feed().has_value());
    // b and c never processed: gone, and told; a goes on
    EXPECT_EQ(r.sink.unprocessed, (std::vector<uint32_t>{b, c}));
    EXPECT_TRUE(r.sink.resets.empty());
    ASSERT_EQ(r.sink.goaways.size(), 1u);
    EXPECT_EQ(r.sink.goaways[0].first, a);
    EXPECT_EQ(r.m.open_streams(), 1u);
    EXPECT_TRUE(r.m.goaway_received());
    EXPECT_FALSE(r.m.can_open());
    EXPECT_EQ(r.m.open_stream(nullptr, 0, true), 0u);
    // a late frame on c is ignored; a answers
    r.data(c, "late", true);
    r.response(a, "200", false);
    r.data(a, "done", true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.sink.body, "done");
    EXPECT_EQ(r.m.open_streams(), 0u);
    EXPECT_EQ(r.last(FrameType::goaway), nullptr);
    // a second GOAWAY may not raise the last stream
    r.w().goaway(0x7FFFFFFF, ErrorCode::no_error);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.m.goaway_last(), a);
}

TEST(H2ClientConnection_Tests, GracefulGoaway_6_8) {
    // Go's first GOAWAY of a shutdown names 2^31 - 1: nothing unprocessed
    Rig r;
    const uint32_t a = r.open("/a");
    r.w().goaway(0x7FFFFFFF, ErrorCode::no_error);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_TRUE(r.sink.unprocessed.empty());
    EXPECT_EQ(r.m.open_streams(), 1u);
    EXPECT_EQ(r.open("/b"), 0u);
    r.w().goaway(a, ErrorCode::no_error);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_TRUE(r.sink.unprocessed.empty());
    EXPECT_EQ(r.m.goaway_last(), a);
}

TEST(H2ClientConnection_Tests, RefusedStream_8_7) {
    Rig r(ClientSettings(), true, {{uint16_t(SettingId::max_concurrent_streams), 1}});
    ASSERT_TRUE(r.feed().has_value());
    const uint32_t id = r.open("/");
    EXPECT_FALSE(r.m.can_open());
    r.w().rst_stream(id, ErrorCode::refused_stream);
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.resets.size(), 1u);
    EXPECT_EQ(r.sink.resets[0], std::make_pair(id, ErrorCode::refused_stream));
    EXPECT_TRUE(r.m.can_open());
    EXPECT_EQ(r.last(FrameType::rst_stream), nullptr);   // never a RST_STREAM for a RST_STREAM (§5.4.2)
}

TEST(H2ClientConnection_Tests, RequestBody_6_9) {
    // the server's window of 65535 a stream: the body waits for its
    // WINDOW_UPDATE
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    const uint32_t id = r.open("/upload", false);
    const std::string body(100000, 'u');
    const size_t first = r.m.send_data(id, reinterpret_cast<const uint8_t*>(body.data()), body.size(), true);
    EXPECT_EQ(first, size_t(DefaultWindow));
    r.w().window_update(id, 50000);
    r.w().window_update(0, 50000);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_NE(std::find(r.sink.windows.begin(), r.sink.windows.end(), id), r.sink.windows.end());
    const size_t rest = r.m.send_data(id, reinterpret_cast<const uint8_t*>(body.data()) + first, body.size() - first, true);
    EXPECT_EQ(rest, body.size() - first);
    r.drain();
    const Out* d = r.last(FrameType::data);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->flags & flag::end_stream, flag::end_stream);
    size_t sent = 0;
    for (auto& o : r.out) {
        if (o.type == FrameType::data) {
            sent += o.payload.size();
            EXPECT_LE(o.payload.size(), DefaultMaxFrameSize);
        }
    }
    EXPECT_EQ(sent, body.size());
}

TEST(H2ClientConnection_Tests, ResponseWindow_6_9_1) {
    // a stream nobody reads stops at its window (1 MB): one byte more is
    // the server's FLOW_CONTROL_ERROR on that stream; the connection's
    // window (16 MB) lets the others go on
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    const uint32_t idle = r.open("/unread");
    const uint32_t busy = r.open("/read");
    r.response(idle, "200", false);
    r.response(busy, "200", false);
    r.data(idle, std::string(1u << 20, 'i'), false);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_TRUE(r.sink.resets.empty());
    // the other stream flows, 4 MB read and consumed as it comes
    const std::string chunk(1u << 18, 'b');
    size_t updates = r.count(FrameType::window_update);
    for (int i = 0; i < 16; ++i) {
        r.data(busy, chunk, i == 15);
        ASSERT_TRUE(r.feed().has_value());
        r.m.consumed(busy, chunk.size());
        r.drain();
    }
    EXPECT_GT(r.count(FrameType::window_update), updates);
    EXPECT_TRUE(r.sink.body_end);
    EXPECT_TRUE(r.sink.resets.empty());
    // the unread one: not a byte more
    r.data(idle, "!", false);
    r.expect_rst(idle, ErrorCode::flow_control_error);
}

TEST(H2ClientConnection_Tests, ResetGivesWindowBack_6_9) {
    // windows at their least: two unread streams fill the connection's;
    // the client gives one up (reset CANCEL) and its bytes go back
    ClientSettings s;
    s.initial_window = DefaultWindow;
    s.connection_window = 2 * DefaultWindow;
    Rig r(s);
    ASSERT_TRUE(r.feed().has_value());
    const uint32_t a = r.open("/a");
    const uint32_t b = r.open("/b");
    const uint32_t c = r.open("/c");
    for (uint32_t id : {a, b, c}) {
        r.response(id, "200", false);
    }
    r.data(a, std::string(DefaultWindow, 'a'), false);
    r.data(b, std::string(DefaultWindow, 'b'), false);
    ASSERT_TRUE(r.feed().has_value());
    const size_t updates = r.count(FrameType::window_update);
    r.m.reset(a, ErrorCode::cancel);
    r.drain();
    const Out* rst = r.last(FrameType::rst_stream);
    ASSERT_NE(rst, nullptr);
    EXPECT_EQ(rst->stream, a);
    EXPECT_EQ(rst->code, uint32_t(ErrorCode::cancel));
    ASSERT_EQ(r.count(FrameType::window_update), updates + 1);
    EXPECT_EQ(r.last(FrameType::window_update)->stream, 0u);
    EXPECT_EQ(r.last(FrameType::window_update)->increment, uint32_t(DefaultWindow));
    // c may now receive; late DATA on a is ignored (its window given back)
    r.data(a, "late", false);
    r.data(c, std::string(1000, 'c'), true);
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.last(FrameType::goaway), nullptr);
    EXPECT_TRUE(r.sink.body_end);
}

TEST(H2ClientConnection_Tests, PeerTableSize_6_5_2) {
    // the server's SETTINGS_HEADER_TABLE_SIZE reaches the encoder: the
    // next request's block starts with a Dynamic Table Size Update
    Rig r(ClientSettings(), true, {{uint16_t(SettingId::header_table_size), 0}});
    ASSERT_TRUE(r.feed().has_value());
    r.open("/a");
    r.drain();
    const Out* h = r.last(FrameType::headers);
    ASSERT_NE(h, nullptr);
    ASSERT_FALSE(h->payload.empty());
    EXPECT_EQ(uint8_t(h->payload[0]), 0x20);   // size update to 0 (RFC 7541 §6.3)
    EXPECT_EQ(r.request_field(":path"), "/a");
    EXPECT_EQ(r.m.encoder().table().count(), 0u);
}

TEST(H2ClientConnection_Tests, Ping_6_7) {
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    const uint8_t mine[8] = {'l', 'i', 'v', 'e', '0', '0', '0', '1'};
    r.m.ping(mine);
    r.drain();
    const Out* p = r.last(FrameType::ping);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->flags, 0);
    r.w().ping(reinterpret_cast<const uint8_t*>(p->payload.data()), true);
    // the server's own: answered
    const uint8_t theirs[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    r.w().ping(theirs, false);
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.pongs.size(), 1u);
    EXPECT_EQ(r.sink.pongs[0], std::string("live0001"));
    const Out* ack = r.last(FrameType::ping);
    EXPECT_EQ(ack->flags, flag::ack);
    EXPECT_EQ(ack->payload, std::string(reinterpret_cast<const char*>(theirs), 8));
}

TEST(H2ClientConnection_Tests, OurGoaway_6_8) {
    // closing an idle connection: GOAWAY NO_ERROR naming no stream of the
    // server's; nothing opens after it
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    r.m.goaway(ErrorCode::no_error);
    r.drain();
    const Out* g = r.last(FrameType::goaway);
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->last, 0u);
    EXPECT_EQ(g->code, uint32_t(ErrorCode::no_error));
    EXPECT_FALSE(r.m.can_open());
    EXPECT_TRUE(r.m.finished());
}

TEST(H2ClientConnection_Tests, Continuation_6_10) {
    // a response's fields across CONTINUATION
    Rig r;
    const uint32_t id = r.open("/");
    std::string b;
    r.enc.begin_block(b);
    r.enc.encode(b, ":status", "200");
    r.enc.encode(b, "x-long", std::string(40, 'l'));
    const uint8_t* p = reinterpret_cast<const uint8_t*>(b.data());
    r.w().headers(id, p, 5, false, false);
    r.w().continuation(id, p + 5, 10, false);
    r.w().continuation(id, p + 15, b.size() - 15, true);
    r.data(id, "", true);
    ASSERT_TRUE(r.feed().has_value());
    ASSERT_EQ(r.sink.responses.size(), 1u);
    EXPECT_EQ(r.sink.responses[0].status, "200");
    EXPECT_TRUE(r.sink.body_end);
    // a frame cutting a block in two: the connection's error
    Rig cut;
    const uint32_t id2 = cut.open("/");
    cut.w().headers(id2, p, 5, false, false);
    cut.data(id2, "x", false);
    cut.expect_goaway(ErrorCode::protocol_error);
}

TEST(H2ClientConnection_Tests, LateResponseAfterCancels_5_1) {
    // requests given up by the client (a timeout each) while the server's
    // answers were on their way: the late HEADERS and DATA are ignored,
    // however many were given up (Go: ignored), and the connection lives
    Rig r;
    ASSERT_TRUE(r.feed().has_value());
    std::vector<uint32_t> ids;
    for (int i = 0; i < 200; ++i) {
        ids.push_back(r.open("/slow"));
    }
    for (uint32_t id : ids) {
        r.m.reset(id, ErrorCode::cancel);
    }
    r.drain();
    for (uint32_t id : ids) {
        r.response(id, "200", false);
        r.data(id, "late", true);
    }
    ASSERT_TRUE(r.feed().has_value());
    EXPECT_EQ(r.last(FrameType::goaway), nullptr);
    EXPECT_TRUE(r.sink.responses.empty());
    EXPECT_NE(r.open("/next"), 0u);
}
