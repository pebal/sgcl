//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The frames of HTTP/2 on any bytes: the first two bytes pick the
// SETTINGS_MAX_FRAME_SIZE in force (2^14 or more), the rest is read frame
// after frame as a connection reads them. What must hold: a frame's views
// lie inside its own bytes; a frame cut short is never read before it is
// whole — a prefix waits for more, or gives the very error the whole
// frame gives (only a header too long can be refused early); and whatever
// reads as a frame of a known type, written again by the writers, reads
// back the same (type, stream, flags that matter, fields, payload).
#include "sgcl/net/http/detail/h2/frame.h"

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

    const uint8_t* bytes(const slice<const byte>& s) {
        return reinterpret_cast<const uint8_t*>(s.data());
    }

    bool same_payload(const Frame& a, const Frame& b) {
        return a.payload.size() == b.payload.size() && (a.payload.size() == 0 || std::memcmp(a.payload.data(), b.payload.data(), a.payload.size()) == 0);
    }

    bool same_priority(const Frame& a, const Frame& b) {
        if (a.priority.has_value() != b.priority.has_value()) {
            return false;
        }
        return !a.priority.has_value() || (a.priority->exclusive == b.priority->exclusive && a.priority->depends_on == b.priority->depends_on &&
                                           a.priority->weight == b.priority->weight);
    }

    std::string rewrite(const Frame& f) {
        std::string out;
        FrameWriter w(out);
        const uint32_t id = f.header.stream;
        switch (f.type()) {
        case FrameType::data:
            w.data(id, bytes(f.payload), f.payload.size(), f.end_stream());
            break;
        case FrameType::headers:
            w.headers(id, bytes(f.payload), f.payload.size(), f.end_stream(), f.end_headers(), f.priority ? &*f.priority : nullptr);
            break;
        case FrameType::priority:
            w.priority(id, *f.priority);
            break;
        case FrameType::rst_stream:
            w.rst_stream(id, ErrorCode(f.error_code));
            break;
        case FrameType::settings:
            if (f.ack()) {
                w.settings_ack();
            } else {
                std::vector<Setting> s;
                for (size_t i = 0; i < f.settings_count(); ++i) {
                    s.push_back(f.setting(i));
                }
                w.settings(s.data(), s.size());
            }
            break;
        case FrameType::push_promise:
            w.push_promise(id, f.promised_stream, bytes(f.payload), f.payload.size(), f.end_headers());
            break;
        case FrameType::ping:
            w.ping(bytes(f.payload), f.ack());
            break;
        case FrameType::goaway:
            w.goaway(f.last_stream, ErrorCode(f.error_code), bytes(f.payload), f.payload.size());
            break;
        case FrameType::window_update:
            w.window_update(id, f.increment);
            break;
        case FrameType::continuation:
            w.continuation(id, bytes(f.payload), f.payload.size(), f.end_headers());
            break;
        default:
            w.header(uint32_t(f.payload.size()), f.header.type, f.header.flags, id);
            out.append(reinterpret_cast<const char*>(f.payload.data()), f.payload.size());
            break;
        }
        return out;
    }

    void same(const Frame& a, const Frame& b) {
        check(a.header.type == b.header.type);
        check(a.header.stream == b.header.stream);
        check(a.end_stream() == b.end_stream());
        check(a.end_headers() == b.end_headers());
        check(a.ack() == b.ack());
        check(same_priority(a, b));
        check(a.error_code == b.error_code);
        check(a.promised_stream == b.promised_stream);
        check(a.last_stream == b.last_stream);
        check(a.increment == b.increment);
        check(same_payload(a, b));
        if (!a.known()) {
            check(a.header.flags == b.header.flags);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    // 2^14, or up to 2^14 + 2^16 - 1 when the first byte is odd
    const uint32_t max = DefaultMaxFrameSize + ((data[0] & 1) ? (uint32_t(data[0]) << 8 | data[1]) : 0);
    data += 2;
    size -= 2;
    size_t at = 0;
    while (at < size) {
        const uint8_t* p = data + at;
        const size_t n = size - at;
        auto r = parse_frame(p, n, max);
        // prefixes shorter than the frame (its header's length, or all
        // there is): at most 64 of them, spread over it
        size_t whole = n;
        if (n >= FrameHeaderSize) {
            whole = std::min<size_t>(n, FrameHeaderSize + read_frame_header(p).length);
        }
        const size_t step = whole / 64 + 1;
        for (size_t k = 0; k < whole; k += step) {
            auto q = parse_frame(p, k, max);
            if (q.has_value()) {
                check(q->size == 0);
            } else {
                check(!r.has_value());
                check(q.error().code == r.error().code && q.error().stream == r.error().stream);
                check(q.error().code == ErrorCode::frame_size_error);   // only the header, read early
            }
        }
        if (!r.has_value()) {
            check(r.error().what != nullptr);
            return 0;
        }
        if (r->size == 0) {
            return 0;
        }
        check(r->size <= n && r->size >= FrameHeaderSize);
        const Frame& f = r->frame;
        check(f.header.length + FrameHeaderSize == r->size);
        check(f.header.length <= max);
        if (f.payload.size()) {
            check(bytes(f.payload) >= p + FrameHeaderSize && bytes(f.payload) + f.payload.size() <= p + r->size);
        }
        const std::string again = rewrite(f);
        auto b = parse_frame(reinterpret_cast<const uint8_t*>(again.data()), again.size(), LargestMaxFrameSize);
        check(b.has_value());
        check(b->size == again.size());
        same(f, b->frame);
        at += r->size;
    }
    return 0;
}
