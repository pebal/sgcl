//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server's HTTP/2 connection machine driven by any bytes: after the
// client's preface and SETTINGS, the input is a script of steps, each a
// byte naming it: bytes for the machine (cut where the script says, the
// rest kept for the next step as a transport keeps it), the server
// answering a stream (HEADERS, DATA), resetting one, ending a handler,
// the transport sending what is waiting, the time passing. The first byte
// picks the limits (few streams or the default, a small header list).
//
// What must hold: the machine's output is always whole frames it may
// send — HEADERS, DATA and RST_STREAM only on streams the client opened,
// never past the peer's frame size, DATA never past the windows; the
// requests come with growing identifiers and none after the last GOAWAY;
// the streams open and the handlers running stay within
// max_concurrent_streams; after a connection error nothing more is read
// and the output ends with GOAWAY.
#include "sgcl/net/http/detail/h2/connection.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http::detail::h2;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Sink {
        std::vector<uint32_t> running;   // handlers started, not yet ended
        std::vector<uint32_t> live;      // streams the server may answer
        uint32_t last_request = 0;
        uint32_t max_id = 0;
        uint32_t goaway_last = 0x7FFFFFFFu;   // the last GOAWAY's, as the transport has sent it

        ErrorCode on_request(uint32_t id, Block&& b, bool, bool start) {
            check(id > last_request);
            check(id & 1);
            check(id <= goaway_last);
            last_request = id;
            if (b.fields.size() && (b.fields.get(":path").size() == 1 && b.fields.get(":path")[0] == '!')) {
                return ErrorCode::protocol_error;   // a request the server refuses
            }
            if (start) {
                running.push_back(id);
            }
            live.push_back(id);
            return ErrorCode::no_error;
        }

        void on_start(uint32_t id) {
            running.push_back(id);
        }

        void on_trailers(uint32_t, Block&&) {
        }

        void on_data(uint32_t, const uint8_t* p, size_t n, bool) {
            volatile uint8_t sink = 0;
            for (size_t i = 0; i < n; ++i) {
                sink = sink ^ p[i];   // the view is read whole: ASan checks it
            }
        }

        void on_reset(uint32_t, ErrorCode) {
        }

        void on_window(uint32_t) {
        }

        void on_goaway(uint32_t, ErrorCode) {
        }

        void on_ping_ack(const uint8_t*) {
        }
    };

    struct Reader {
        const uint8_t* p;
        size_t n;

        bool empty() const {
            return n == 0;
        }

        uint8_t byte() {
            if (!n) {
                return 0;
            }
            --n;
            return *p++;
        }
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Reader in{data, size};
    const uint8_t pick = in.byte();
    ServerSettings s;
    if (pick & 1) {
        s.max_concurrent_streams = 1 + (pick >> 5);
    }
    if (pick & 2) {
        s.max_header_list_size = 256;
    }
    if (pick & 4) {
        s.initial_window = DefaultWindow;
        s.connection_window = DefaultWindow;
    }
    Sink sink;
    ServerConnection<Sink> m(sink, s);
    int64_t now = 1;
    m.start(now);
    std::string wire(Preface, PrefaceSize);
    FrameWriter(wire).settings(nullptr, 0);
    bool failed = false;
    uint32_t peer_frame = DefaultMaxFrameSize;
    std::vector<uint8_t> payload(70000, 'b');

    auto feed = [&]() {
        auto r = m.feed(reinterpret_cast<const uint8_t*>(wire.data()), wire.size(), now);
        if (!r.has_value()) {
            check(r.error().what != nullptr);
            failed = true;
            // nothing more read
            auto again = m.feed(reinterpret_cast<const uint8_t*>(wire.data()), wire.size(), now);
            check(!again.has_value());
            wire.clear();
            return;
        }
        check(*r <= wire.size());
        wire.erase(0, *r);
    };

    // DATA sent in place (the server's body blocks): the output taken with
    // take_output, its pieces put back among its bytes, each piece inside
    // the payload it was given from
    const bool in_place = (pick & 8) != 0;
    std::string taken_bytes;
    std::vector<ServerConnection<Sink>::OutPiece> taken_pieces;
    std::string whole;
    auto output = [&]() -> slice<const byte> {
        if (!in_place) {
            return m.output();
        }
        m.take_output(taken_bytes, taken_pieces);
        whole.clear();
        size_t at = 0;
        for (auto& q : taken_pieces) {
            check(q.at >= at && q.at <= taken_bytes.size());
            check(q.p >= payload.data() && q.p + q.n <= payload.data() + payload.size());
            whole.append(taken_bytes, at, q.at - at);
            whole.append(reinterpret_cast<const char*>(q.p), q.n);
            at = q.at;
        }
        whole.append(taken_bytes, at, std::string::npos);
        return slice<const byte>(reinterpret_cast<const byte*>(whole.data()), whole.size());
    };

    // everything waiting read as frames and checked, then sent
    auto flush = [&]() {
        auto o = output();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(o.data());
        size_t at = 0;
        bool goaway_last_frame = false;
        while (at < o.size()) {
            auto f = parse_frame(p + at, o.size() - at, LargestMaxFrameSize);
            check(f.has_value());
            check(f->size > 0);
            const Frame& fr = f->frame;
            const uint32_t id = fr.header.stream;
            goaway_last_frame = fr.type() == FrameType::goaway;
            switch (fr.type()) {
            case FrameType::headers:
            case FrameType::continuation:
            case FrameType::data:
            case FrameType::rst_stream:
                // a RST_STREAM may answer a bad PRIORITY on any stream
                // named; the rest go only to the client's streams
                check(id != 0 && id <= sink.max_id);
                check((id & 1) || fr.type() == FrameType::rst_stream);
                check(fr.header.length <= peer_frame || fr.type() == FrameType::rst_stream);
                break;
            case FrameType::goaway:
                sink.goaway_last = fr.last_stream;
                break;
            default:
                break;
            }
            at += f->size;
        }
        if (failed) {
            check(goaway_last_frame || o.size() == 0);
        }
        if (!in_place) {
            m.written(o.size());
        }
    };

    feed();
    flush();
    while (!in.empty() && !failed) {
        const uint8_t op = in.byte();
        switch (op & 7) {
        case 0:
        case 1:
        case 2:
        case 3: {
            // bytes for the machine: the step's length, then the bytes
            size_t n = in.byte();
            if (op & 0x80) {
                n = n << 6 | in.byte();
            }
            n = std::min(n, in.n);
            // the highest client stream a frame header names, for the checks
            wire.append(reinterpret_cast<const char*>(in.p), n);
            in.p += n;
            in.n -= n;
            for (size_t k = 0; k + FrameHeaderSize <= wire.size();) {
                const FrameHeader h = read_frame_header(reinterpret_cast<const uint8_t*>(wire.data()) + k);
                const bool opens = h.type == uint8_t(FrameType::headers) || h.type == uint8_t(FrameType::priority);
                if (opens && h.stream > sink.max_id) {
                    sink.max_id = h.stream;
                }
                if (h.type == uint8_t(FrameType::settings) && k + FrameHeaderSize + h.length <= wire.size() && !(h.flags & flag::ack)) {
                    const uint8_t* e = reinterpret_cast<const uint8_t*>(wire.data()) + k + FrameHeaderSize;
                    for (size_t j = 0; j + 6 <= h.length; j += 6) {
                        if ((uint16_t(e[j]) << 8 | e[j + 1]) == uint16_t(SettingId::max_frame_size)) {
                            const uint32_t v = read32(e + j + 2);
                            if (v >= DefaultMaxFrameSize && v <= LargestMaxFrameSize) {
                                // the largest the peer has named bounds the checks
                                peer_frame = std::max(peer_frame, v);
                            }
                        }
                    }
                }
                k += FrameHeaderSize + h.length;
            }
            feed();
            break;
        }
        case 4: {
            // the server answers a stream: a field block, maybe many frames long
            if (sink.live.empty()) {
                break;
            }
            const uint32_t id = sink.live[in.byte() % sink.live.size()];
            const size_t n = (op & 0x80) ? 20000 : 1;
            std::vector<uint8_t> block(n, 0x88);
            m.send_headers(id, block.data(), block.size(), (op & 0x40) != 0);
            break;
        }
        case 5: {
            if (sink.live.empty()) {
                break;
            }
            const uint32_t id = sink.live[in.byte() % sink.live.size()];
            const size_t n = size_t(in.byte()) << 8;
            const int64_t before = m.connection_send_window();
            const size_t k = std::min(n, payload.size());
            const size_t taken = in_place ? m.send_data_in_place(id, payload.data(), k, (op & 0x40) != 0) : m.send_data(id, payload.data(), k, (op & 0x40) != 0);
            check(int64_t(taken) <= std::max<int64_t>(before, 0));
            break;
        }
        case 6: {
            // a handler ends, or the server resets its stream
            if (sink.running.empty()) {
                break;
            }
            const size_t i = in.byte() % sink.running.size();
            const uint32_t id = sink.running[i];
            if (op & 0x40) {
                m.reset(id, ErrorCode::cancel);
            }
            if (op & 0x80) {
                m.consumed(id, in.byte());
            }
            sink.running.erase(sink.running.begin() + i);
            m.release(id);
            break;
        }
        default:
            if (op & 0x40) {
                now += int64_t(in.byte()) * 1'000'000'000;
                if (auto t = m.tick(now); !t.has_value()) {
                    failed = true;
                }
            }
            if (op & 0x80) {
                m.drain();
            }
            flush();
            break;
        }
        check(m.open_streams() <= s.max_concurrent_streams);
        check(m.handlers() <= s.max_concurrent_streams);
        check(m.connection_send_window() <= int64_t(LargestWindow));
    }
    flush();
    return 0;
}
