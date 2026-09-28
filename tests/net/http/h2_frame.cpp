//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http/2: the frames, pure, by vectors (sgcl/net/http/detail/h2/frame.h).
// One test per section of RFC 9113 §4 and §6, named by it; each rule of a
// section as a frame written by hand, byte by byte from the RFC's figures,
// with the error the RFC names for breaking it (its code, and whether the
// connection or the stream). Then the writers against the same figures,
// every frame cut at every byte (nothing is read before it is whole, save
// a header too long), and a deterministic mutator over valid frames whose
// seeds are in the loop: no read outside the bytes, and whatever reads
// back writes the same.
#include "tests/types.h"
#include "sgcl/net/http/detail/h2/frame.h"

#include <deque>
#include <random>
#include <string>

using namespace sgcl;
using namespace sgcl::net::http::detail::h2;

namespace {
    std::string raw(uint32_t length, uint8_t type, uint8_t flags, uint32_t stream, const std::string& payload) {
        std::string s;
        s += char(length >> 16);
        s += char(length >> 8);
        s += char(length);
        s += char(type);
        s += char(flags);
        s += char(stream >> 24);
        s += char(stream >> 16);
        s += char(stream >> 8);
        s += char(stream);
        return s + payload;
    }

    std::string raw(uint8_t type, uint8_t flags, uint32_t stream, const std::string& payload) {
        return raw(uint32_t(payload.size()), type, flags, stream, payload);
    }

    std::string raw(FrameType type, uint8_t flags, uint32_t stream, const std::string& payload) {
        return raw(uint8_t(type), flags, stream, payload);
    }

    std::string u32(uint32_t v) {
        return std::string{char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
    }

    std::string zeros(size_t n) {
        return std::string(n, '\0');
    }

    std::string setting(uint16_t id, uint32_t v) {
        return std::string{char(id >> 8), char(id)} + u32(v);
    }

    expected<Parsed, Error> parse(const std::string& s, uint32_t max = DefaultMaxFrameSize) {
        return parse_frame(reinterpret_cast<const uint8_t*>(s.data()), s.size(), max);
    }

    std::string text(const slice<const byte>& p) {
        return std::string(reinterpret_cast<const char*>(p.data()), p.size());
    }

    // the error a frame must give: its code and its stream (0: the connection)
    void refused(const std::string& s, ErrorCode code, uint32_t stream) {
        auto r = parse(s);
        ASSERT_FALSE(r.has_value()) << "accepted";
        EXPECT_EQ(r.error().code, code) << r.error().what;
        EXPECT_EQ(r.error().stream, stream) << r.error().what;
        EXPECT_NE(r.error().what, nullptr);
    }

    // a frame's payload is a view of its bytes: the bytes of accepted
    // frames are kept for the test's whole run
    Frame accepted(const std::string& bytes) {
        static std::deque<std::string> kept;
        const std::string& s = kept.emplace_back(bytes);
        auto r = parse(s);
        EXPECT_TRUE(r.has_value()) << (r.has_value() ? "" : r.error().what);
        if (!r.has_value()) {
            return Frame();
        }
        EXPECT_EQ(r->size, s.size());
        return r->frame;
    }
}

TEST(H2Frame_Tests, Preface_3_4) {
    EXPECT_EQ(PrefaceSize, 24u);
    EXPECT_EQ(std::string(Preface, PrefaceSize), std::string("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"));
}

TEST(H2Frame_Tests, Header_4_1) {
    // length 3 in 24 bits, type, flags, the reserved bit set and dropped
    std::string s = raw(3, 0x0, 0x1, 0x80000005u, "abc");
    auto r = parse(s);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->size, 12u);
    EXPECT_EQ(r->frame.header.length, 3u);
    EXPECT_EQ(r->frame.header.stream, 5u);
    EXPECT_TRUE(r->frame.end_stream());
    EXPECT_EQ(text(r->frame.payload), "abc");
    // two frames back to back: the first only
    auto two = parse(s + raw(FrameType::ping, 0, 0, std::string(8, 'x')));
    ASSERT_TRUE(two.has_value());
    EXPECT_EQ(two->size, 12u);
}

TEST(H2Frame_Tests, Incomplete_4_1) {
    std::string s = raw(FrameType::data, 0, 1, "hello");
    for (size_t n = 0; n < s.size(); ++n) {
        auto r = parse(s.substr(0, n));
        ASSERT_TRUE(r.has_value()) << n;
        EXPECT_EQ(r->size, 0u) << n;
    }
}

TEST(H2Frame_Tests, Size_4_2) {
    // 2^14 is always allowed; one more past the setting is refused from the
    // header alone, before its payload has come
    std::string ok = raw(FrameType::data, 0, 1, std::string(16384, 'a'));
    EXPECT_TRUE(parse(ok).has_value());
    std::string head = raw(16385, 0x0, 0, 1, "");
    auto r = parse(head);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, ErrorCode::frame_size_error);
    EXPECT_TRUE(r.error().connection());
    // a larger SETTINGS_MAX_FRAME_SIZE in force lets it through
    auto big = parse(raw(FrameType::data, 0, 1, std::string(16385, 'a')), 1 << 20);
    ASSERT_TRUE(big.has_value());
    EXPECT_EQ(big->frame.payload.size(), 16385u);
    // the largest header length, refused under any setting below it
    auto most = parse(raw(0xFFFFFF, 0x0, 0, 1, ""), LargestMaxFrameSize - 1);
    ASSERT_FALSE(most.has_value());
    EXPECT_EQ(most.error().code, ErrorCode::frame_size_error);
}

TEST(H2Frame_Tests, UnknownType_4_1_5_5) {
    // passed over whatever its stream and flags
    Frame f = accepted(raw(0xFA, 0xFF, 0, "anything"));
    EXPECT_FALSE(f.known());
    EXPECT_EQ(text(f.payload), "anything");
    EXPECT_FALSE(f.end_stream());
    EXPECT_FALSE(f.ack());
    accepted(raw(0x0A, 0, 7, ""));
}

TEST(H2Frame_Tests, Data_6_1) {
    Frame f = accepted(raw(FrameType::data, flag::end_stream, 1, "body"));
    EXPECT_EQ(f.type(), FrameType::data);
    EXPECT_TRUE(f.end_stream());
    EXPECT_EQ(text(f.payload), "body");
    // padded: Pad Length, data, padding
    f = accepted(raw(FrameType::data, flag::padded, 3, std::string("\x03", 1) + "xy" + std::string(3, '\0')));
    EXPECT_EQ(text(f.payload), "xy");
    EXPECT_FALSE(f.end_stream());
    // padding the whole rest: empty data, allowed
    f = accepted(raw(FrameType::data, flag::padded, 3, std::string("\x02", 1) + std::string(2, '\0')));
    EXPECT_EQ(f.payload.size(), 0u);
    // padding past the payload
    refused(raw(FrameType::data, flag::padded, 3, std::string("\x03", 1) + "ab"), ErrorCode::protocol_error, 0);
    refused(raw(FrameType::data, flag::padded, 3, std::string("\xFF", 1)), ErrorCode::protocol_error, 0);
    // PADDED with no Pad Length
    refused(raw(FrameType::data, flag::padded, 3, ""), ErrorCode::frame_size_error, 0);
    // stream 0
    refused(raw(FrameType::data, 0, 0, "x"), ErrorCode::protocol_error, 0);
    // flags it does not define are ignored (§4.1)
    f = accepted(raw(FrameType::data, 0xF6, 1, "z"));
    EXPECT_FALSE(f.end_stream());
    EXPECT_EQ(text(f.payload), "z");
}

TEST(H2Frame_Tests, Headers_6_2) {
    Frame f = accepted(raw(FrameType::headers, flag::end_headers | flag::end_stream, 1, "\x82\x86"));
    EXPECT_EQ(f.type(), FrameType::headers);
    EXPECT_TRUE(f.end_headers());
    EXPECT_TRUE(f.end_stream());
    EXPECT_FALSE(f.priority.has_value());
    EXPECT_EQ(text(f.payload), "\x82\x86");
    // priority: E, dependency, weight, taken off the block
    f = accepted(raw(FrameType::headers, flag::priority, 5, u32(0x80000003u) + "\x0F" + "blk"));
    ASSERT_TRUE(f.priority.has_value());
    EXPECT_TRUE(f.priority->exclusive);
    EXPECT_EQ(f.priority->depends_on, 3u);
    EXPECT_EQ(f.priority->weight, 15);
    EXPECT_EQ(text(f.payload), "blk");
    EXPECT_FALSE(f.end_headers());
    // padded and priority: Pad Length first, the padding at the end
    f = accepted(raw(FrameType::headers, flag::priority | flag::padded, 5, std::string("\x02", 1) + u32(1) + zeros(1) + "blk" + zeros(2)));
    EXPECT_EQ(text(f.payload), "blk");
    EXPECT_EQ(f.priority->depends_on, 1u);
    // padding may not reach into the priority fields
    refused(raw(FrameType::headers, flag::priority | flag::padded, 5, std::string("\x03", 1) + u32(1) + std::string("\x00", 1)),
            ErrorCode::protocol_error, 0);
    // too short for the priority
    refused(raw(FrameType::headers, flag::priority, 5, "abcd"), ErrorCode::frame_size_error, 0);
    // depending on itself: read (its block must be decoded), marked; the
    // connection gives the stream's error
    f = accepted(raw(FrameType::headers, flag::priority, 5, u32(5) + "\x10"));
    EXPECT_TRUE(f.self_dependent());
    EXPECT_FALSE(accepted(raw(FrameType::headers, flag::priority, 5, u32(3) + "\x10")).self_dependent());
    // stream 0
    refused(raw(FrameType::headers, flag::end_headers, 0, "\x82"), ErrorCode::protocol_error, 0);
}

TEST(H2Frame_Tests, Priority_6_3) {
    Frame f = accepted(raw(FrameType::priority, 0, 7, u32(3) + "\xFF"));
    ASSERT_TRUE(f.priority.has_value());
    EXPECT_FALSE(f.priority->exclusive);
    EXPECT_EQ(f.priority->depends_on, 3u);
    EXPECT_EQ(f.priority->weight, 255);
    refused(raw(FrameType::priority, 0, 0, u32(3) + "\x01"), ErrorCode::protocol_error, 0);
    // a length other than 5: the stream's FRAME_SIZE_ERROR
    refused(raw(FrameType::priority, 0, 7, u32(3)), ErrorCode::frame_size_error, 7);
    refused(raw(FrameType::priority, 0, 7, u32(3) + "\x01\x02"), ErrorCode::frame_size_error, 7);
    refused(raw(FrameType::priority, 0, 7, u32(7) + "\x01"), ErrorCode::protocol_error, 7);
}

TEST(H2Frame_Tests, RstStream_6_4) {
    Frame f = accepted(raw(FrameType::rst_stream, 0, 3, u32(uint32_t(ErrorCode::cancel))));
    EXPECT_EQ(f.error_code, uint32_t(ErrorCode::cancel));
    // an unknown code is carried as it is (§7)
    EXPECT_EQ(accepted(raw(FrameType::rst_stream, 0, 3, u32(0xDEAD))).error_code, 0xDEADu);
    refused(raw(FrameType::rst_stream, 0, 0, u32(0)), ErrorCode::protocol_error, 0);
    refused(raw(FrameType::rst_stream, 0, 3, zeros(3)), ErrorCode::frame_size_error, 0);
    refused(raw(FrameType::rst_stream, 0, 3, u32(0) + "x"), ErrorCode::frame_size_error, 0);
}

TEST(H2Frame_Tests, Settings_6_5) {
    std::string entries = setting(1, 4096) + setting(2, 0) + setting(3, 100) + setting(4, 65535) + setting(5, 16384) + setting(6, 8192) + setting(0x77, 5);
    Frame f = accepted(raw(FrameType::settings, 0, 0, entries));
    ASSERT_EQ(f.settings_count(), 7u);
    EXPECT_EQ(f.setting(0).id, 1);
    EXPECT_EQ(f.setting(0).value, 4096u);
    EXPECT_EQ(f.setting(2).value, 100u);
    EXPECT_EQ(f.setting(6).id, 0x77);   // unknown: carried, the connection passes it over
    EXPECT_FALSE(f.ack());
    // empty settings are allowed
    EXPECT_EQ(accepted(raw(FrameType::settings, 0, 0, "")).settings_count(), 0u);
    // ACK: empty
    f = accepted(raw(FrameType::settings, flag::ack, 0, ""));
    EXPECT_TRUE(f.ack());
    refused(raw(FrameType::settings, flag::ack, 0, setting(1, 1)), ErrorCode::frame_size_error, 0);
    // on a stream
    refused(raw(FrameType::settings, 0, 1, ""), ErrorCode::protocol_error, 0);
    // not a multiple of 6
    refused(raw(FrameType::settings, 0, 0, setting(1, 1) + "x"), ErrorCode::frame_size_error, 0);
    refused(raw(FrameType::settings, 0, 0, "abcde"), ErrorCode::frame_size_error, 0);
}

TEST(H2Frame_Tests, SettingValues_6_5_2) {
    auto one = [](uint16_t id, uint32_t v) {
        return raw(FrameType::settings, 0, 0, setting(id, v));
    };
    accepted(one(2, 1));
    refused(one(2, 2), ErrorCode::protocol_error, 0);
    accepted(one(4, LargestWindow));
    refused(one(4, LargestWindow + 1), ErrorCode::flow_control_error, 0);
    accepted(one(5, 16384));
    accepted(one(5, LargestMaxFrameSize));
    refused(one(5, 16383), ErrorCode::protocol_error, 0);
    refused(one(5, LargestMaxFrameSize + 1), ErrorCode::protocol_error, 0);
    // any value for the rest, and for an unknown id
    accepted(one(1, 0xFFFFFFFFu));
    accepted(one(3, 0));
    accepted(one(6, 0xFFFFFFFFu));
    accepted(one(0, 0xFFFFFFFFu));
    // a bad value after good ones
    refused(raw(FrameType::settings, 0, 0, setting(1, 0) + setting(3, 1) + setting(2, 7)), ErrorCode::protocol_error, 0);
}

TEST(H2Frame_Tests, PushPromise_6_6) {
    Frame f = accepted(raw(FrameType::push_promise, flag::end_headers, 1, u32(0x80000002u) + "blk"));
    EXPECT_EQ(f.promised_stream, 2u);
    EXPECT_EQ(text(f.payload), "blk");
    EXPECT_TRUE(f.end_headers());
    f = accepted(raw(FrameType::push_promise, flag::padded, 1, std::string("\x01", 1) + u32(4) + "b" + zeros(1)));
    EXPECT_EQ(f.promised_stream, 4u);
    EXPECT_EQ(text(f.payload), "b");
    // padding into the promised stream
    refused(raw(FrameType::push_promise, flag::padded, 1, std::string("\x02", 1) + u32(4) + "b"), ErrorCode::protocol_error, 0);
    refused(raw(FrameType::push_promise, 0, 1, "abc"), ErrorCode::frame_size_error, 0);
    refused(raw(FrameType::push_promise, 0, 0, u32(2)), ErrorCode::protocol_error, 0);
}

TEST(H2Frame_Tests, Ping_6_7) {
    Frame f = accepted(raw(FrameType::ping, 0, 0, "12345678"));
    EXPECT_EQ(text(f.payload), "12345678");
    EXPECT_FALSE(f.ack());
    EXPECT_TRUE(accepted(raw(FrameType::ping, flag::ack, 0, "12345678")).ack());
    refused(raw(FrameType::ping, 0, 1, "12345678"), ErrorCode::protocol_error, 0);
    refused(raw(FrameType::ping, 0, 0, "1234567"), ErrorCode::frame_size_error, 0);
    refused(raw(FrameType::ping, 0, 0, "123456789"), ErrorCode::frame_size_error, 0);
}

TEST(H2Frame_Tests, Goaway_6_8) {
    Frame f = accepted(raw(FrameType::goaway, 0, 0, u32(0x80000007u) + u32(uint32_t(ErrorCode::enhance_your_calm)) + "debug"));
    EXPECT_EQ(f.last_stream, 7u);
    EXPECT_EQ(f.error_code, uint32_t(ErrorCode::enhance_your_calm));
    EXPECT_EQ(text(f.payload), "debug");
    EXPECT_EQ(accepted(raw(FrameType::goaway, 0, 0, u32(0) + u32(0))).payload.size(), 0u);
    refused(raw(FrameType::goaway, 0, 1, u32(0) + u32(0)), ErrorCode::protocol_error, 0);
    refused(raw(FrameType::goaway, 0, 0, u32(0) + "abc"), ErrorCode::frame_size_error, 0);
}

TEST(H2Frame_Tests, WindowUpdate_6_9) {
    EXPECT_EQ(accepted(raw(FrameType::window_update, 0, 0, u32(1))).increment, 1u);
    EXPECT_EQ(accepted(raw(FrameType::window_update, 0, 9, u32(0xFFFFFFFFu))).increment, LargestWindow);   // the reserved bit dropped
    // an increment of 0: the connection's error on stream 0, the stream's on a stream
    refused(raw(FrameType::window_update, 0, 0, u32(0)), ErrorCode::protocol_error, 0);
    refused(raw(FrameType::window_update, 0, 9, u32(0)), ErrorCode::protocol_error, 9);
    refused(raw(FrameType::window_update, 0, 9, u32(0x80000000u)), ErrorCode::protocol_error, 9);
    refused(raw(FrameType::window_update, 0, 9, zeros(2) + "\1"), ErrorCode::frame_size_error, 0);
    refused(raw(FrameType::window_update, 0, 0, u32(1) + "x"), ErrorCode::frame_size_error, 0);
}

TEST(H2Frame_Tests, Continuation_6_10) {
    Frame f = accepted(raw(FrameType::continuation, flag::end_headers, 1, "rest"));
    EXPECT_TRUE(f.end_headers());
    EXPECT_EQ(text(f.payload), "rest");
    // PADDED is not a flag of CONTINUATION: the byte is part of the block
    EXPECT_EQ(text(accepted(raw(FrameType::continuation, flag::padded, 1, "\x05z")).payload), "\x05z");
    refused(raw(FrameType::continuation, 0, 0, "rest"), ErrorCode::protocol_error, 0);
}

TEST(H2Frame_Tests, Writers_6) {
    std::string out;
    FrameWriter w(out);
    const uint8_t body[] = {'h', 'i'};
    w.data(1, body, 2, true);
    EXPECT_EQ(out, raw(FrameType::data, flag::end_stream, 1, "hi"));
    out.clear();
    w.headers(3, body, 2, false, true);
    EXPECT_EQ(out, raw(FrameType::headers, flag::end_headers, 3, "hi"));
    out.clear();
    Priority p{true, 1, 200};
    w.headers(3, body, 2, true, false, &p);
    EXPECT_EQ(out, raw(FrameType::headers, flag::end_stream | flag::priority, 3, u32(0x80000001u) + "\xC8" + "hi"));
    out.clear();
    w.continuation(3, body, 2, true);
    EXPECT_EQ(out, raw(FrameType::continuation, flag::end_headers, 3, "hi"));
    out.clear();
    w.priority(5, Priority{false, 3, 16});
    EXPECT_EQ(out, raw(FrameType::priority, 0, 5, u32(3) + "\x10"));
    out.clear();
    w.rst_stream(5, ErrorCode::refused_stream);
    EXPECT_EQ(out, raw(FrameType::rst_stream, 0, 5, u32(7)));
    out.clear();
    Setting s[] = {{3, 250}, {4, 1 << 20}};
    w.settings(s, 2);
    EXPECT_EQ(out, raw(FrameType::settings, 0, 0, setting(3, 250) + setting(4, 1 << 20)));
    out.clear();
    w.settings_ack();
    EXPECT_EQ(out, raw(FrameType::settings, flag::ack, 0, ""));
    out.clear();
    const uint8_t opaque[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    w.ping(opaque, true);
    EXPECT_EQ(out, raw(FrameType::ping, flag::ack, 0, "\1\2\3\4\5\6\7\x08"));
    out.clear();
    w.goaway(9, ErrorCode::enhance_your_calm, body, 2);
    EXPECT_EQ(out, raw(FrameType::goaway, 0, 0, u32(9) + u32(0xb) + "hi"));
    out.clear();
    w.window_update(0, 1 << 16);
    EXPECT_EQ(out, raw(FrameType::window_update, 0, 0, u32(1 << 16)));
    out.clear();
    w.push_promise(1, 2, body, 2, true);
    EXPECT_EQ(out, raw(FrameType::push_promise, flag::end_headers, 1, u32(2) + "hi"));
    // what is written reads back
    out.clear();
    w.data(1, body, 2, false);
    w.settings(s, 2);
    w.window_update(7, 100);
    size_t at = 0;
    int frames = 0;
    while (at < out.size()) {
        auto r = parse_frame(reinterpret_cast<const uint8_t*>(out.data()) + at, out.size() - at, DefaultMaxFrameSize);
        ASSERT_TRUE(r.has_value());
        ASSERT_GT(r->size, 0u);
        at += r->size;
        ++frames;
    }
    EXPECT_EQ(frames, 3);
}

TEST(H2Frame_Tests, Mutated) {
    // valid frames of every type, then bytes changed, dropped and repeated
    // by a generator whose seed is the loop's index: every verdict is a
    // frame read inside its bytes or an error with a text; whatever reads
    // as a known frame is read again the same from the bytes it took
    const std::string valid[] = {
        raw(FrameType::data, flag::padded | flag::end_stream, 1, std::string("\x02", 1) + "abc" + zeros(2)),
        raw(FrameType::headers, flag::priority | flag::padded | flag::end_headers, 3, std::string("\x01", 1) + u32(1) + "\x05" + "\x82\x86" + zeros(1)),
        raw(FrameType::priority, 0, 3, u32(1) + "\x05"),
        raw(FrameType::rst_stream, 0, 3, u32(8)),
        raw(FrameType::settings, 0, 0, setting(4, 1 << 20) + setting(5, 1 << 15)),
        raw(FrameType::push_promise, flag::padded, 1, std::string("\x01", 1) + u32(2) + "x" + zeros(1)),
        raw(FrameType::ping, 0, 0, "abcdefgh"),
        raw(FrameType::goaway, 0, 0, u32(3) + u32(1) + "dbg"),
        raw(FrameType::window_update, 0, 1, u32(10)),
        raw(FrameType::continuation, flag::end_headers, 1, "\x84"),
    };
    for (unsigned seed = 0; seed < 20000; ++seed) {
        std::mt19937 g(seed);
        std::string s = valid[seed % std::size(valid)];
        const int edits = 1 + int(g() % 4);
        for (int e = 0; e < edits && !s.empty(); ++e) {
            size_t at = g() % s.size();
            switch (g() % 3) {
            case 0: s[at] = char(g()); break;
            case 1: s.erase(at, 1); break;
            default: s.insert(at, 1, s[at]); break;
            }
        }
        auto r = parse(s, uint32_t(DefaultMaxFrameSize + (g() % 2) * 100000));
        if (!r.has_value()) {
            ASSERT_NE(r.error().what, nullptr) << seed;
            continue;
        }
        if (r->size == 0) {
            continue;
        }
        ASSERT_LE(r->size, s.size()) << seed;
        const Frame& f = r->frame;
        if (f.payload.size()) {
            const byte* b = reinterpret_cast<const byte*>(s.data());
            ASSERT_GE(f.payload.data(), b + FrameHeaderSize) << seed;
            ASSERT_LE(f.payload.data() + f.payload.size(), b + r->size) << seed;
        }
    }
}
