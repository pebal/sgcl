//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's HTTP/2 connection machine (client_connection.h) driven by
// any bytes: the input is a script of steps, each a byte naming it: bytes
// from the server (cut where the script says, the rest kept for the next
// step as a transport keeps it), the client opening a stream (a request
// encoded with the machine's encoder, with or without a body), sending a
// body's bytes, taking a body's bytes or giving a stream up, PING, GOAWAY,
// the transport sending what is waiting, the time passing. The first byte
// picks the limits (few streams, a small header list, the least windows).
//
// What must hold: the machine's output is the preface and then whole
// frames it may send — HEADERS, CONTINUATION and DATA only on streams it
// opened, never past the server's frame size, DATA never past the windows;
// the identifiers it gives rise and are odd, none after the server's
// GOAWAY; a response's DATA and trailers come only after its final fields,
// on a stream the client opened; a stream the server never processed is
// above the GOAWAY's last; the streams open stay within ours; after a
// connection error nothing more is read and the output ends with GOAWAY.
#include "sgcl/net/http/detail/h2/client_connection.h"

#include <algorithm>
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
        std::vector<uint32_t> opened;   // the client's streams, maybe gone
        std::vector<uint32_t> final;    // the final fields came
        uint32_t goaway_last = 0x7FFFFFFFu;

        bool is_opened(uint32_t id) const {
            return std::find(opened.begin(), opened.end(), id) != opened.end();
        }

        bool is_final(uint32_t id) const {
            return std::find(final.begin(), final.end(), id) != final.end();
        }

        ErrorCode on_response(uint32_t id, Block&& b, bool, bool informational) {
            check(is_opened(id));
            check(!is_final(id));
            if (!informational) {
                final.push_back(id);
            }
            // a response without :status is refused, as the transport will
            return b.fields.get(":status").size() == 3 ? ErrorCode::no_error : ErrorCode::protocol_error;
        }

        void on_trailers(uint32_t id, Block&&) {
            check(is_final(id));
        }

        void on_data(uint32_t id, const uint8_t* p, size_t n, bool) {
            check(is_final(id));
            volatile uint8_t x = 0;
            for (size_t i = 0; i < n; ++i) {
                x = x ^ p[i];   // the view is read whole: ASan checks it
            }
        }

        void on_reset(uint32_t id, ErrorCode) {
            check(is_opened(id));
        }

        void on_unprocessed(uint32_t id) {
            check(is_opened(id));
            check(id > goaway_last);
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
    ClientSettings s;
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
    if (pick & 8) {
        s.initial_concurrent_streams = 2;
    }
    Sink sink;
    ClientConnection<Sink> m(sink, s);
    int64_t now = 1;
    m.start(now);
    std::string wire;
    bool failed = false;
    bool preface_seen = false;
    uint32_t peer_frame = DefaultMaxFrameSize;
    uint32_t last_id = 0;
    std::vector<uint8_t> payload(70000, 'q');

    auto feed = [&]() {
        auto r = m.feed(reinterpret_cast<const uint8_t*>(wire.data()), wire.size(), now);
        if (!r.has_value()) {
            check(r.error().what != nullptr);
            failed = true;
            auto again = m.feed(reinterpret_cast<const uint8_t*>(wire.data()), wire.size(), now);
            check(!again.has_value());
            wire.clear();
            return;
        }
        check(*r <= wire.size());
        wire.erase(0, *r);
    };

    // a request's body sent in place (the client's bodies in memory,
    // send_data_held): the output taken with take_output, its pieces put
    // back among its bytes, each piece inside the payload it came from
    // (the bit is shared with the stream count's, a choice either way)
    const bool in_place = (pick & 0x20) != 0;
    std::string taken_bytes;
    sgcl::vector<ClientConnection<Sink>::OutPiece> taken_pieces;
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
            const uint8_t* qp = reinterpret_cast<const uint8_t*>(q.piece.data());
            check(!q.piece.owned() && qp >= payload.data() && qp + q.piece.size() <= payload.data() + payload.size());
            whole.append(taken_bytes, at, q.at - at);
            whole.append(reinterpret_cast<const char*>(qp), q.piece.size());
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
        if (!preface_seen) {
            check(o.size() >= PrefaceSize && std::memcmp(p, Preface, PrefaceSize) == 0);
            at = PrefaceSize;
            preface_seen = true;
        }
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
                check(sink.is_opened(id));
                check(fr.header.length <= peer_frame);
                break;
            case FrameType::rst_stream:
                // our streams, or a stream the server named wrongly
                check(id != 0);
                break;
            case FrameType::goaway:
                check(fr.last_stream == 0);   // the server opens none
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

    // the server's SETTINGS first, most of the time
    if (!(pick & 16)) {
        FrameWriter(wire).settings(nullptr, 0);
        feed();
    }
    flush();
    while (!in.empty() && !failed) {
        const uint8_t op = in.byte();
        switch (op & 7) {
        case 0:
        case 1:
        case 2: {
            // bytes from the server: the step's length, then the bytes
            size_t n = in.byte();
            if (op & 0x80) {
                n = n << 6 | in.byte();
            }
            n = std::min(n, in.n);
            wire.append(reinterpret_cast<const char*>(in.p), n);
            in.p += n;
            in.n -= n;
            for (size_t k = 0; k + FrameHeaderSize <= wire.size();) {
                const FrameHeader h = read_frame_header(reinterpret_cast<const uint8_t*>(wire.data()) + k);
                if (h.type == uint8_t(FrameType::settings) && k + FrameHeaderSize + h.length <= wire.size() && !(h.flags & flag::ack)) {
                    const uint8_t* e = reinterpret_cast<const uint8_t*>(wire.data()) + k + FrameHeaderSize;
                    for (size_t j = 0; j + 6 <= h.length; j += 6) {
                        if ((uint16_t(e[j]) << 8 | e[j + 1]) == uint16_t(SettingId::max_frame_size)) {
                            const uint32_t v = read32(e + j + 2);
                            if (v >= DefaultMaxFrameSize && v <= LargestMaxFrameSize) {
                                peer_frame = std::max(peer_frame, v);
                            }
                        }
                    }
                }
                if (h.type == uint8_t(FrameType::goaway) && k + FrameHeaderSize + h.length <= wire.size() && h.length >= 8 && h.stream == 0) {
                    const uint32_t last = read32(reinterpret_cast<const uint8_t*>(wire.data()) + k + FrameHeaderSize) & 0x7FFFFFFFu;
                    sink.goaway_last = std::min(sink.goaway_last, last);
                }
                k += FrameHeaderSize + h.length;
            }
            feed();
            break;
        }
        case 3: {
            // a request: encoded only when a stream may open (the
            // encoder's table must stay the server's)
            const bool could = m.can_open();
            if (!could) {
                check(m.open_stream(nullptr, 0, true) == 0);
                break;
            }
            check(!m.goaway_received());
            std::string block;
            Encoder& e = m.encoder();
            e.begin_block(block);
            e.encode(block, ":method", (op & 0x40) ? "POST" : "GET");
            e.encode(block, ":scheme", "https");
            e.encode(block, ":authority", "example.com");
            e.encode(block, ":path", std::string(1 + in.byte() % 40, 'p'));
            if (op & 0x80) {
                e.encode(block, "x-big", std::string(size_t(in.byte()) * 100, 'b'));   // CONTINUATION past a frame
            }
            const uint32_t id = m.open_stream(reinterpret_cast<const uint8_t*>(block.data()), block.size(), (op & 0x40) == 0);
            check(id != 0 && (id & 1) && id > last_id);
            last_id = id;
            sink.opened.push_back(id);
            break;
        }
        case 4: {
            // a request's body
            if (sink.opened.empty()) {
                break;
            }
            const uint32_t id = sink.opened[in.byte() % sink.opened.size()];
            const size_t n = size_t(in.byte()) << 8;
            const int64_t before = m.connection_send_window();
            const size_t k = std::min(n, payload.size());
            const size_t taken = in_place ? m.send_data_in_place(id, slice<const byte>(reinterpret_cast<const byte*>(payload.data()), k), (op & 0x40) != 0) : m.send_data(id, payload.data(), k, (op & 0x40) != 0);
            check(int64_t(taken) <= std::max<int64_t>(before, 0));
            break;
        }
        case 5: {
            // a body's bytes taken, or the stream given up
            if (sink.opened.empty()) {
                break;
            }
            const uint32_t id = sink.opened[in.byte() % sink.opened.size()];
            if (op & 0x40) {
                m.reset(id, ErrorCode::cancel);
            } else {
                m.consumed(id, size_t(in.byte()) << 6);
            }
            break;
        }
        case 6: {
            if (op & 0x40) {
                const uint8_t opaque[8] = {1, 2, 3, 4, 5, 6, 7, op};
                m.ping(opaque);
            }
            if (op & 0x80) {
                m.goaway(ErrorCode::no_error);
            }
            flush();
            break;
        }
        default:
            if (op & 0x40) {
                now += int64_t(in.byte()) * 1'000'000'000;
                if (auto t = m.tick(now); !t.has_value()) {
                    failed = true;
                }
            }
            flush();
            break;
        }
        check(m.open_streams() <= std::max(1u, s.max_concurrent_streams));
        check(m.stream_limit() <= std::max(1u, s.max_concurrent_streams));
        check(m.connection_send_window() <= int64_t(LargestWindow));
        if (m.goaway_received()) {
            check(!m.can_open());
        }
    }
    flush();
    return 0;
}
