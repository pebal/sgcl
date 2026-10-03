//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../../../../core/aliases.h"
#include "../../../../core/expected.h"
#include "../../../../core/slice.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// The frames of HTTP/2 (RFC 9113 §4, §6): the frame header, the ten frame
// types, each read into a view of its own bytes with the rules of §6
// checked — the stream identifier a type must or must not have, the
// lengths a type allows, padding, the settings' values (§6.5.2) — and
// written into a buffer. A frame longer than the SETTINGS_MAX_FRAME_SIZE in
// force is refused from its header alone, before its payload is waited for
// (§4.2). A frame of an unknown type is read and passed over (§4.1, §5.5).
//
// What a frame breaks is an Error: of the connection (GOAWAY, stream 0) or
// of one stream (RST_STREAM), with its code (§7). What depends on the state
// of the connection or of a stream (a frame on a closed stream, a
// CONTINUATION that does not follow its HEADERS, the flow-control windows)
// is the connection machine's to decide, not the reader's.
//
// Reading makes no copy: a Frame's payload is a view of the bytes given,
// valid while they are; it lives in a frame of the stack (a slice), never
// in an object that outlives the bytes.
namespace sgcl::net::http::detail::h2 {
    enum class FrameType : uint8_t {
        data = 0x0,
        headers = 0x1,
        priority = 0x2,
        rst_stream = 0x3,
        settings = 0x4,
        push_promise = 0x5,
        ping = 0x6,
        goaway = 0x7,
        window_update = 0x8,
        continuation = 0x9,
    };

    // The flags of §6 (END_STREAM and ACK share a bit, each in its types)
    namespace flag {
        inline constexpr uint8_t end_stream = 0x1;
        inline constexpr uint8_t ack = 0x1;
        inline constexpr uint8_t end_headers = 0x4;
        inline constexpr uint8_t padded = 0x8;
        inline constexpr uint8_t priority = 0x20;
    }

    // §6.5.2
    enum class SettingId : uint16_t {
        header_table_size = 0x1,
        enable_push = 0x2,
        max_concurrent_streams = 0x3,
        initial_window_size = 0x4,
        max_frame_size = 0x5,
        max_header_list_size = 0x6,
    };

    inline constexpr size_t FrameHeaderSize = 9;
    inline constexpr uint32_t DefaultMaxFrameSize = 16384;          // 2^14 (§4.2)
    inline constexpr uint32_t LargestMaxFrameSize = (1u << 24) - 1;
    inline constexpr uint32_t LargestWindow = (1u << 31) - 1;       // §6.9.1
    inline constexpr uint32_t DefaultWindow = 65535;                // §6.9.2

    // The connection preface of the client (§3.4), 24 bytes
    inline constexpr char Preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    inline constexpr size_t PrefaceSize = sizeof(Preface) - 1;

    struct FrameHeader {
        uint32_t length = 0;     // 24 bits
        uint8_t type = 0;        // a FrameType, or an unknown one
        uint8_t flags = 0;
        uint32_t stream = 0;     // 31 bits, the reserved bit dropped (§4.1)

        bool has(uint8_t f) const noexcept {
            return (flags & f) != 0;
        }
    };

    inline FrameHeader read_frame_header(const uint8_t* p) noexcept {
        FrameHeader h;
        h.length = uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2];
        h.type = p[3];
        h.flags = p[4];
        h.stream = (uint32_t(p[5]) << 24 | uint32_t(p[6]) << 16 | uint32_t(p[7]) << 8 | p[8]) & 0x7FFFFFFFu;
        return h;
    }

    inline uint32_t read32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
    }

    // The priority of a HEADERS or PRIORITY frame (§5.3.2, §6.3): read and
    // checked, nothing done with it (§5.3: RFC 9113 leaves it to the peer)
    struct Priority {
        bool exclusive = false;
        uint32_t depends_on = 0;
        uint8_t weight = 0;      // the weight less 1
    };

    // One setting (§6.5.1)
    struct Setting {
        uint16_t id = 0;
        uint32_t value = 0;
    };

    // A frame read: its header and what its type carries. `payload` is the
    // DATA's data, the field block fragment of HEADERS, PUSH_PROMISE and
    // CONTINUATION (padding and priority taken off), the settings' entries,
    // PING's eight bytes, GOAWAY's debug data, an unknown type's payload
    struct Frame {
        FrameHeader header;
        slice<const byte> payload;
        optional<Priority> priority;
        uint32_t error_code = 0;       // RST_STREAM, GOAWAY
        uint32_t promised_stream = 0;  // PUSH_PROMISE
        uint32_t last_stream = 0;      // GOAWAY
        uint32_t increment = 0;        // WINDOW_UPDATE

        FrameType type() const noexcept {
            return FrameType(header.type);
        }

        bool known() const noexcept {
            return header.type <= uint8_t(FrameType::continuation);
        }

        bool end_stream() const noexcept {
            return (type() == FrameType::data || type() == FrameType::headers) && header.has(flag::end_stream);
        }

        bool end_headers() const noexcept {
            return (type() == FrameType::headers || type() == FrameType::push_promise || type() == FrameType::continuation) && header.has(flag::end_headers);
        }

        bool ack() const noexcept {
            return (type() == FrameType::settings || type() == FrameType::ping) && header.has(flag::ack);
        }

        // HEADERS or PRIORITY naming its own stream as its dependency
        // (§5.3.1): PRIORITY is refused by the reader, HEADERS read for its
        // field block, the error then the connection's to give
        bool self_dependent() const noexcept {
            return priority.has_value() && priority->depends_on == header.stream;
        }

        // SETTINGS: the entries, each six bytes
        size_t settings_count() const noexcept {
            return payload.size() / 6;
        }

        Setting setting(size_t i) const noexcept {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(payload.data()) + 6 * i;
            return Setting{uint16_t(uint16_t(p[0]) << 8 | p[1]), read32(p + 2)};
        }
    };

    // A frame at the front of the bytes: `size` the bytes it takes (0 when
    // more are needed; the header alone is enough to refuse a frame too
    // long), `frame` what it carries
    struct Parsed {
        size_t size = 0;
        Frame frame;
    };

    namespace frame_detail {
        inline slice<const byte> view(const uint8_t* p, size_t n) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(p), n);
        }

        // Padding (§6.1, §6.2, §6.6): the Pad Length byte taken off the
        // front; a padded frame without it is too short (FRAME_SIZE_ERROR)
        inline bool pad_length(const FrameHeader& h, const uint8_t*& p, size_t& n, size_t& pad, Error& e) noexcept {
            pad = 0;
            if (!h.has(flag::padded)) {
                return true;
            }
            if (n < 1) {
                e = connection_error(ErrorCode::frame_size_error, "a padded frame without its Pad Length");
                return false;
            }
            pad = p[0];
            ++p;
            --n;
            return true;
        }

        // The padding taken off the end, after the fixed fields: padding
        // that reaches into them or past the payload is a PROTOCOL_ERROR
        // of the connection
        inline bool unpad(size_t pad, size_t& n, Error& e) noexcept {
            if (pad > n) {
                e = connection_error(ErrorCode::protocol_error, "padding as long as the frame's payload");
                return false;
            }
            n -= pad;
            return true;
        }
    }

    // The frame at the front of `bytes`, its payload checked by §6; a frame
    // longer than max_frame_size is a FRAME_SIZE_ERROR of the connection
    // (§4.2) from its header alone
    [[nodiscard]] inline expected<Parsed, Error> parse_frame(const uint8_t* bytes, size_t size, uint32_t max_frame_size) noexcept {
        using namespace frame_detail;
        Parsed out;
        if (size < FrameHeaderSize) {
            return out;
        }
        const FrameHeader h = read_frame_header(bytes);
        if (h.length > max_frame_size) {
            return unexpected(connection_error(ErrorCode::frame_size_error, "a frame longer than SETTINGS_MAX_FRAME_SIZE"));
        }
        if (size < FrameHeaderSize + h.length) {
            return out;
        }
        out.size = FrameHeaderSize + h.length;
        Frame& f = out.frame;
        f.header = h;
        const uint8_t* p = bytes + FrameHeaderSize;
        size_t n = h.length;
        Error e;
        size_t pad = 0;
        switch (FrameType(h.type)) {
        case FrameType::data:
            // §6.1
            if (h.stream == 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "DATA on stream 0"));
            }
            if (!pad_length(h, p, n, pad, e) || !unpad(pad, n, e)) {
                return unexpected(e);
            }
            f.payload = view(p, n);
            break;
        case FrameType::headers:
            // §6.2
            if (h.stream == 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "HEADERS on stream 0"));
            }
            if (!pad_length(h, p, n, pad, e)) {
                return unexpected(e);
            }
            if (h.has(flag::priority)) {
                if (n < 5) {
                    return unexpected(connection_error(ErrorCode::frame_size_error, "HEADERS too short for its priority"));
                }
                Priority pr;
                const uint32_t d = read32(p);
                pr.exclusive = (d >> 31) != 0;
                pr.depends_on = d & 0x7FFFFFFFu;
                pr.weight = p[4];
                // depending on itself is the stream's PROTOCOL_ERROR (§5.3.1),
                // but its field block must still be decoded (§4.3): the
                // frame is read and the connection gives the error
                f.priority = pr;
                p += 5;
                n -= 5;
            }
            if (!unpad(pad, n, e)) {
                return unexpected(e);
            }
            f.payload = view(p, n);
            break;
        case FrameType::priority: {
            // §6.3
            if (h.stream == 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "PRIORITY on stream 0"));
            }
            if (n != 5) {
                return unexpected(stream_error(h.stream, ErrorCode::frame_size_error, "PRIORITY of a length other than 5"));
            }
            Priority pr;
            const uint32_t d = read32(p);
            pr.exclusive = (d >> 31) != 0;
            pr.depends_on = d & 0x7FFFFFFFu;
            pr.weight = p[4];
            if (pr.depends_on == h.stream) {
                return unexpected(stream_error(h.stream, ErrorCode::protocol_error, "a stream that depends on itself"));
            }
            f.priority = pr;
            break;
        }
        case FrameType::rst_stream:
            // §6.4
            if (h.stream == 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "RST_STREAM on stream 0"));
            }
            if (n != 4) {
                return unexpected(connection_error(ErrorCode::frame_size_error, "RST_STREAM of a length other than 4"));
            }
            f.error_code = read32(p);
            break;
        case FrameType::settings:
            // §6.5
            if (h.stream != 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "SETTINGS on a stream"));
            }
            if (h.has(flag::ack)) {
                if (n != 0) {
                    return unexpected(connection_error(ErrorCode::frame_size_error, "a SETTINGS acknowledgement with a payload"));
                }
                break;
            }
            if (n % 6 != 0) {
                return unexpected(connection_error(ErrorCode::frame_size_error, "SETTINGS of a length not a multiple of 6"));
            }
            f.payload = view(p, n);
            for (size_t i = 0; i < f.settings_count(); ++i) {
                const Setting s = f.setting(i);
                switch (SettingId(s.id)) {
                case SettingId::enable_push:
                    if (s.value > 1) {
                        return unexpected(connection_error(ErrorCode::protocol_error, "SETTINGS_ENABLE_PUSH other than 0 or 1"));
                    }
                    break;
                case SettingId::initial_window_size:
                    if (s.value > LargestWindow) {
                        return unexpected(connection_error(ErrorCode::flow_control_error, "SETTINGS_INITIAL_WINDOW_SIZE past 2^31 - 1"));
                    }
                    break;
                case SettingId::max_frame_size:
                    if (s.value < DefaultMaxFrameSize || s.value > LargestMaxFrameSize) {
                        return unexpected(connection_error(ErrorCode::protocol_error, "SETTINGS_MAX_FRAME_SIZE outside 2^14 .. 2^24 - 1"));
                    }
                    break;
                default:
                    break;   // the others take any value; an unknown one is passed over
                }
            }
            break;
        case FrameType::push_promise:
            // §6.6: read; whether one may come at all is the connection's
            if (h.stream == 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "PUSH_PROMISE on stream 0"));
            }
            if (!pad_length(h, p, n, pad, e)) {
                return unexpected(e);
            }
            if (n < 4) {
                return unexpected(connection_error(ErrorCode::frame_size_error, "PUSH_PROMISE too short for its stream"));
            }
            f.promised_stream = read32(p) & 0x7FFFFFFFu;
            p += 4;
            n -= 4;
            if (!unpad(pad, n, e)) {
                return unexpected(e);
            }
            f.payload = view(p, n);
            break;
        case FrameType::ping:
            // §6.7
            if (h.stream != 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "PING on a stream"));
            }
            if (n != 8) {
                return unexpected(connection_error(ErrorCode::frame_size_error, "PING of a length other than 8"));
            }
            f.payload = view(p, 8);
            break;
        case FrameType::goaway:
            // §6.8
            if (h.stream != 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "GOAWAY on a stream"));
            }
            if (n < 8) {
                return unexpected(connection_error(ErrorCode::frame_size_error, "GOAWAY shorter than 8"));
            }
            f.last_stream = read32(p) & 0x7FFFFFFFu;
            f.error_code = read32(p + 4);
            f.payload = view(p + 8, n - 8);
            break;
        case FrameType::window_update:
            // §6.9
            if (n != 4) {
                return unexpected(connection_error(ErrorCode::frame_size_error, "WINDOW_UPDATE of a length other than 4"));
            }
            f.increment = read32(p) & 0x7FFFFFFFu;
            if (f.increment == 0) {
                return unexpected(h.stream == 0 ? connection_error(ErrorCode::protocol_error, "WINDOW_UPDATE of 0 on the connection")
                                                : stream_error(h.stream, ErrorCode::protocol_error, "WINDOW_UPDATE of 0 on a stream"));
            }
            break;
        case FrameType::continuation:
            // §6.10
            if (h.stream == 0) {
                return unexpected(connection_error(ErrorCode::protocol_error, "CONTINUATION on stream 0"));
            }
            f.payload = view(p, n);
            break;
        default:
            f.payload = view(p, n);   // an unknown type: passed over by the connection (§5.5)
            break;
        }
        return out;
    }

    // --- writing ------------------------------------------------------------

    // Frames appended to a buffer the connection keeps (a std::string, as
    // HPACK's encoder writes its blocks); the frame header
    // first, the payload after it. A payload past 2^24 - 1, a stream
    // identifier past 2^31 - 1, a PING of other than 8 bytes are mistakes
    // of the program (asserted)
    class FrameWriter {
    public:
        explicit FrameWriter(std::string& out) noexcept
        : _out(&out) {
        }

        void header(uint32_t length, FrameType type, uint8_t flags, uint32_t stream) noexcept {
            header(length, uint8_t(type), flags, stream);
        }

        void header(uint32_t length, uint8_t type, uint8_t flags, uint32_t stream) noexcept {
            const char h[9] = {char(length >> 16), char(length >> 8), char(length), char(type), char(flags),
                               char((stream >> 24) & 0x7F), char(stream >> 16), char(stream >> 8), char(stream)};
            _out->append(h, 9);
        }

        void data(uint32_t stream, const uint8_t* p, size_t n, bool end_stream) noexcept {
            header(uint32_t(n), FrameType::data, end_stream ? flag::end_stream : 0, stream);
            _append(p, n);
        }

        void headers(uint32_t stream, const uint8_t* block, size_t n, bool end_stream, bool end_headers, const Priority* priority = nullptr) noexcept {
            const uint8_t flags = uint8_t((end_stream ? flag::end_stream : 0) | (end_headers ? flag::end_headers : 0) | (priority ? flag::priority : 0));
            header(uint32_t(n + (priority ? 5 : 0)), FrameType::headers, flags, stream);
            if (priority) {
                _priority(*priority);
            }
            _append(block, n);
        }

        void push_promise(uint32_t stream, uint32_t promised, const uint8_t* block, size_t n, bool end_headers) noexcept {
            header(uint32_t(4 + n), FrameType::push_promise, end_headers ? flag::end_headers : 0, stream);
            _u32(promised & 0x7FFFFFFFu);
            _append(block, n);
        }

        void continuation(uint32_t stream, const uint8_t* block, size_t n, bool end_headers) noexcept {
            header(uint32_t(n), FrameType::continuation, end_headers ? flag::end_headers : 0, stream);
            _append(block, n);
        }

        void priority(uint32_t stream, const Priority& p) noexcept {
            header(5, FrameType::priority, 0, stream);
            _priority(p);
        }

        void rst_stream(uint32_t stream, ErrorCode code) noexcept {
            header(4, FrameType::rst_stream, 0, stream);
            _u32(uint32_t(code));
        }

        void settings(const Setting* s, size_t n) noexcept {
            header(uint32_t(6 * n), FrameType::settings, 0, 0);
            for (size_t i = 0; i < n; ++i) {
                _out->push_back(char(s[i].id >> 8));
                _out->push_back(char(s[i].id));
                _u32(s[i].value);
            }
        }

        void settings_ack() noexcept {
            header(0, FrameType::settings, flag::ack, 0);
        }

        void ping(const uint8_t data[8], bool ack) noexcept {
            header(8, FrameType::ping, ack ? flag::ack : 0, 0);
            _append(data, 8);
        }

        void goaway(uint32_t last_stream, ErrorCode code, const uint8_t* debug = nullptr, size_t n = 0) noexcept {
            header(uint32_t(8 + n), FrameType::goaway, 0, 0);
            _u32(last_stream & 0x7FFFFFFFu);
            _u32(uint32_t(code));
            _append(debug, n);
        }

        void window_update(uint32_t stream, uint32_t increment) noexcept {
            header(4, FrameType::window_update, 0, stream);
            _u32(increment & 0x7FFFFFFFu);
        }

    private:
        std::string* _out;

        void _append(const uint8_t* p, size_t n) noexcept {
            if (n) {
                _out->append(reinterpret_cast<const char*>(p), n);
            }
        }

        void _priority(const Priority& p) noexcept {
            _u32((p.exclusive ? 0x80000000u : 0) | (p.depends_on & 0x7FFFFFFFu));
            _out->push_back(char(p.weight));
        }

        void _u32(uint32_t v) noexcept {
            const char b[4] = {char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
            _out->append(b, 4);
        }
    };
}
