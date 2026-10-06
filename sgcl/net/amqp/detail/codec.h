//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../types.h"
#include "../../../core/aliases.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"

#include <bit>
#include <cstdint>
#include <string>
#include <string_view>

// AMQP 0-9-1's wire (§4.2): frames, the domains of the methods' arguments,
// field tables, the basic class's properties. Written into a std::string,
// read from a view with a flag that a short read sets
namespace sgcl::net::amqp::detail {
    inline constexpr uint8_t FrameMethod = 1;
    inline constexpr uint8_t FrameHeader = 2;
    inline constexpr uint8_t FrameBody = 3;
    inline constexpr uint8_t FrameHeartbeat = 8;
    inline constexpr uint8_t FrameEnd = 0xCE;
    inline constexpr std::string_view ProtocolHeader{"AMQP\x00\x00\x09\x01", 8};
    inline constexpr uint32_t FrameMin = 4096;   // the smallest frame_max either side may ask (§4.2.3)
    inline constexpr int TableDepth = 32;        // tables in tables: past it the frame is malformed

    // The classes and methods (§1.9 and RabbitMQ's extensions)
    namespace m {
        inline constexpr uint32_t id(uint16_t c, uint16_t i) noexcept {
            return uint32_t(c) << 16 | i;
        }
        inline constexpr uint32_t connection_start = id(10, 10), connection_start_ok = id(10, 11), connection_secure = id(10, 20),
                                  connection_secure_ok = id(10, 21), connection_tune = id(10, 30), connection_tune_ok = id(10, 31),
                                  connection_open = id(10, 40), connection_open_ok = id(10, 41), connection_close = id(10, 50),
                                  connection_close_ok = id(10, 51), connection_blocked = id(10, 60), connection_unblocked = id(10, 61);
        inline constexpr uint32_t channel_open = id(20, 10), channel_open_ok = id(20, 11), channel_flow = id(20, 20), channel_flow_ok = id(20, 21),
                                  channel_close = id(20, 40), channel_close_ok = id(20, 41);
        inline constexpr uint32_t exchange_declare = id(40, 10), exchange_declare_ok = id(40, 11), exchange_delete = id(40, 20),
                                  exchange_delete_ok = id(40, 21);
        inline constexpr uint32_t queue_declare = id(50, 10), queue_declare_ok = id(50, 11), queue_bind = id(50, 20), queue_bind_ok = id(50, 21),
                                  queue_purge = id(50, 30), queue_purge_ok = id(50, 31), queue_delete = id(50, 40), queue_delete_ok = id(50, 41),
                                  queue_unbind = id(50, 50), queue_unbind_ok = id(50, 51);
        inline constexpr uint32_t basic_qos = id(60, 10), basic_qos_ok = id(60, 11), basic_consume = id(60, 20), basic_consume_ok = id(60, 21),
                                  basic_cancel = id(60, 30), basic_cancel_ok = id(60, 31), basic_publish = id(60, 40), basic_return = id(60, 50),
                                  basic_deliver = id(60, 60), basic_get = id(60, 70), basic_get_ok = id(60, 71), basic_get_empty = id(60, 72),
                                  basic_ack = id(60, 80), basic_reject = id(60, 90), basic_recover_async = id(60, 100), basic_recover = id(60, 110),
                                  basic_recover_ok = id(60, 111), basic_nack = id(60, 120);
        inline constexpr uint32_t confirm_select = id(85, 10), confirm_select_ok = id(85, 11);
    }

    // The arguments of a method written in their order; consecutive bits
    // packed into an octet, the first the lowest (§4.2.5.2)
    class AmqpWriter {
    public:
        explicit AmqpWriter(std::string& out) noexcept
        : _out(out) {
        }

        void u8(uint8_t v) {
            _flush_bits();
            _out += char(v);
        }

        void u16(uint16_t v) {
            _flush_bits();
            char b[2] = {char(v >> 8), char(v)};
            _out.append(b, 2);
        }

        void u32(uint32_t v) {
            _flush_bits();
            char b[4] = {char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
            _out.append(b, 4);
        }

        void u64(uint64_t v) {
            u32(uint32_t(v >> 32));
            u32(uint32_t(v));
        }

        // A shortstr: 255 bytes at most (the caller checks the length of a
        // name it was given; the rest is cut)
        void shortstr(std::string_view s) {
            _flush_bits();
            size_t n = s.size() > 255 ? 255 : s.size();
            _out += char(n);
            _out.append(s.data(), n);
        }

        void longstr(std::string_view s) {
            u32(uint32_t(s.size()));
            _out.append(s.data(), s.size());
        }

        void bit(bool v) {
            if (_nbits == 8) {
                _flush_bits();
            }
            _bits |= uint8_t(v) << _nbits;
            ++_nbits;
        }

        void table(const amqp::table& t) {
            _flush_bits();
            size_t at = _out.size();
            u32(0);
            for (auto& [name, v] : t) {
                shortstr(name.view());
                value(v);
            }
            _patch32(at, uint32_t(_out.size() - at - 4));
        }

        void value(const field& v) {
            using K = field::kind;
            switch (v.type()) {
                case K::none: _out += 'V'; break;
                case K::boolean: _out += 't'; u8(*v.as_bool()); break;
                case K::int8: _out += 'b'; u8(uint8_t(*v.as_int())); break;
                case K::uint8: _out += 'B'; u8(uint8_t(*v.as_int())); break;
                case K::int16: _out += 's'; u16(uint16_t(*v.as_int())); break;
                case K::uint16: _out += 'u'; u16(uint16_t(*v.as_int())); break;
                case K::int32: _out += 'I'; u32(uint32_t(*v.as_int())); break;
                case K::uint32: _out += 'i'; u32(uint32_t(*v.as_int())); break;
                case K::int64: _out += 'l'; u64(uint64_t(*v.as_int())); break;
                case K::float32: {
                    _out += 'f';
                    u32(std::bit_cast<uint32_t>(float(*v.as_double())));
                    break;
                }
                case K::float64: {
                    _out += 'd';
                    u64(std::bit_cast<uint64_t>(*v.as_double()));
                    break;
                }
                case K::decimal: _out += 'D'; u8(v.scale()); u32(uint32_t(v._i)); break;
                case K::string: _out += 'S'; longstr(v.as_string()->view()); break;
                case K::bytes: _out += 'x'; longstr(v.as_string()->view()); break;
                case K::timestamp: _out += 'T'; u64(uint64_t(v.as_timestamp()->unix())); break;
                case K::array: {
                    _out += 'A';
                    size_t at = _out.size();
                    u32(0);
                    auto items = v.as_array();
                    for (auto& item : *items) {
                        value(item);
                    }
                    _patch32(at, uint32_t(_out.size() - at - 4));
                    break;
                }
                case K::table: _out += 'F'; table(*v.as_table()); break;
            }
        }

        // The basic class's properties: the flags, then the ones present
        // (§4.2.6.1; content-type is bit 15)
        void properties(const amqp::properties& p) {
            uint16_t flags = 0;
            flags |= uint16_t(!p.content_type.empty()) << 15;
            flags |= uint16_t(!p.content_encoding.empty()) << 14;
            flags |= uint16_t(!p.headers.empty()) << 13;
            flags |= uint16_t(p.delivery_mode != delivery_mode::none) << 12;
            flags |= uint16_t(p.priority != 0) << 11;
            flags |= uint16_t(!p.correlation_id.empty()) << 10;
            flags |= uint16_t(!p.reply_to.empty()) << 9;
            flags |= uint16_t(!p.expiration.empty()) << 8;
            flags |= uint16_t(!p.message_id.empty()) << 7;
            flags |= uint16_t(p.timestamp.has_value()) << 6;
            flags |= uint16_t(!p.type.empty()) << 5;
            flags |= uint16_t(!p.user_id.empty()) << 4;
            flags |= uint16_t(!p.app_id.empty()) << 3;
            u16(flags);
            if (flags & 0x8000) shortstr(p.content_type.view());
            if (flags & 0x4000) shortstr(p.content_encoding.view());
            if (flags & 0x2000) table(p.headers);
            if (flags & 0x1000) u8(uint8_t(p.delivery_mode));
            if (flags & 0x0800) u8(p.priority);
            if (flags & 0x0400) shortstr(p.correlation_id.view());
            if (flags & 0x0200) shortstr(p.reply_to.view());
            if (flags & 0x0100) shortstr(p.expiration.view());
            if (flags & 0x0080) shortstr(p.message_id.view());
            if (flags & 0x0040) u64(uint64_t(p.timestamp->unix()));
            if (flags & 0x0020) shortstr(p.type.view());
            if (flags & 0x0010) shortstr(p.user_id.view());
            if (flags & 0x0008) shortstr(p.app_id.view());
        }

        void finish() {
            _flush_bits();
        }

    private:
        void _flush_bits() {
            if (_nbits) {
                _out += char(_bits);
                _bits = 0;
                _nbits = 0;
            }
        }

        void _patch32(size_t at, uint32_t v) noexcept {
            _out[at] = char(v >> 24);
            _out[at + 1] = char(v >> 16);
            _out[at + 2] = char(v >> 8);
            _out[at + 3] = char(v);
        }

        std::string& _out;
        uint8_t _bits = 0;
        int _nbits = 0;
    };

    // A timestamp's seconds as a datetime: past the ±292 years a datetime
    // holds, its last second (a broker's u64 may say anything)
    inline time::datetime amqp_time(uint64_t seconds) noexcept {
        constexpr int64_t Limit = 9223372035;
        int64_t v = int64_t(seconds);
        v = v > Limit ? Limit : v < -Limit ? -Limit : v;
        return time::datetime::from_unix(v, time::zone::utc());
    }

    // The arguments read in their order; a read past the end clears ok and
    // gives zero or empty from then on
    class AmqpReader {
    public:
        explicit AmqpReader(std::string_view b) noexcept
        : _b(b) {
        }

        bool ok = true;

        bool done() const noexcept {
            return _at == _b.size();
        }

        uint8_t u8() noexcept {
            _nbits = 0;
            if (_at >= _b.size()) {
                ok = false;
                return 0;
            }
            return uint8_t(_b[_at++]);
        }

        uint16_t u16() noexcept {
            _nbits = 0;
            if (_b.size() - _at < 2) {
                return _short();
            }
            uint16_t v = uint16_t(uint8_t(_b[_at]) << 8 | uint8_t(_b[_at + 1]));
            _at += 2;
            return v;
        }

        uint32_t u32() noexcept {
            _nbits = 0;
            if (_b.size() - _at < 4) {
                return _short();
            }
            uint32_t v = uint32_t(uint8_t(_b[_at])) << 24 | uint32_t(uint8_t(_b[_at + 1])) << 16 | uint32_t(uint8_t(_b[_at + 2])) << 8 | uint8_t(_b[_at + 3]);
            _at += 4;
            return v;
        }

        uint64_t u64() noexcept {
            uint64_t hi = u32();
            return hi << 32 | u32();
        }

        std::string_view shortstr() noexcept {
            size_t n = u8();
            return _take(n);
        }

        std::string_view longstr() noexcept {
            size_t n = u32();
            return _take(n);
        }

        bool bit() noexcept {
            if (_nbits == 0 || _nbits == 8) {
                if (_at >= _b.size()) {
                    ok = false;
                    return false;
                }
                _bits = uint8_t(_b[_at++]);
                _nbits = 0;
            }
            return (_bits >> _nbits++) & 1;
        }

        amqp::table table(int depth = 0) {
            amqp::table out;
            std::string_view body = longstr();
            if (!ok) {
                return out;
            }
            if (depth >= TableDepth) {
                ok = false;
                return out;
            }
            AmqpReader r(body);
            while (r.ok && !r.done()) {
                string name(r.shortstr());
                field v = r.value(depth + 1);
                if (r.ok) {
                    out.push_back({std::move(name), std::move(v)});
                }
            }
            ok = r.ok;
            return out;
        }

        field value(int depth) {
            _nbits = 0;
            if (depth >= TableDepth) {
                ok = false;
                return field();
            }
            switch (char(u8())) {
                case 'V': return field();
                case 't': return field(u8() != 0);
                case 'b': return field::int8(int8_t(u8()));
                case 'B': return field::uint8(u8());
                case 's': return field::int16(int16_t(u16()));
                case 'u': return field::uint16(u16());
                case 'I': return field(int32_t(u32()));
                case 'i': return field::uint32(u32());
                case 'l': return field(int64_t(u64()));
                case 'L': return field(int64_t(u64()));   // 0-9-1's own uint64: RabbitMQ never writes it
                case 'f': {
                    return field::float32(std::bit_cast<float>(u32()));
                }
                case 'd': {
                    return field(std::bit_cast<double>(u64()));
                }
                case 'D': {
                    uint8_t scale = u8();
                    int32_t v = int32_t(u32());
                    if (scale > 9) {
                        ok = false;
                        return field();
                    }
                    return field::decimal(scale, v);
                }
                case 'S': return field(string(longstr()));
                case 'x': return field::bytes(string(longstr()));
                case 'T': return field::timestamp(amqp_time(u64()));
                case 'A': {
                    std::string_view body = longstr();
                    vector<field> items;
                    AmqpReader r(body);
                    while (ok && r.ok && !r.done()) {
                        field v = r.value(depth + 1);
                        if (r.ok) {
                            items.push_back(std::move(v));
                        }
                    }
                    ok = ok && r.ok;
                    return field(items);
                }
                case 'F': return field(table(depth + 1));
                default:
                    ok = false;
                    return field();
            }
        }

        amqp::properties properties() {
            amqp::properties p;
            uint16_t flags = u16();
            if (flags & 1) {
                ok = false;   // a second flags word: none of the basic class's properties needs one
                return p;
            }
            if (flags & 0x8000) p.content_type = string(shortstr());
            if (flags & 0x4000) p.content_encoding = string(shortstr());
            if (flags & 0x2000) p.headers = table();
            if (flags & 0x1000) p.delivery_mode = amqp::delivery_mode(u8());
            if (flags & 0x0800) p.priority = u8();
            if (flags & 0x0400) p.correlation_id = string(shortstr());
            if (flags & 0x0200) p.reply_to = string(shortstr());
            if (flags & 0x0100) p.expiration = string(shortstr());
            if (flags & 0x0080) p.message_id = string(shortstr());
            if (flags & 0x0040) p.timestamp = amqp_time(u64());
            if (flags & 0x0020) p.type = string(shortstr());
            if (flags & 0x0010) p.user_id = string(shortstr());
            if (flags & 0x0008) p.app_id = string(shortstr());
            if (flags & 0x0004) (void)shortstr();   // cluster-id, deprecated
            return p;
        }

        std::string_view rest() noexcept {
            auto r = _b.substr(_at);
            _at = _b.size();
            return r;
        }

    private:
        uint32_t _short() noexcept {
            ok = false;
            _at = _b.size();
            return 0;
        }

        std::string_view _take(size_t n) noexcept {
            if (!ok || _b.size() - _at < n) {
                ok = false;
                _at = _b.size();
                return {};
            }
            auto r = _b.substr(_at, n);
            _at += n;
            return r;
        }

        std::string_view _b;
        size_t _at = 0;
        uint8_t _bits = 0;
        int _nbits = 0;
    };

    // A frame's head at the buffer's front: 1 with its type, channel and
    // payload's size, 0 when fewer than 7 bytes wait
    inline int amqp_frame_head(std::string_view b, uint8_t& type, uint16_t& channel, uint32_t& size) noexcept {
        if (b.size() < 7) {
            return 0;
        }
        type = uint8_t(b[0]);
        channel = uint16_t(uint8_t(b[1]) << 8 | uint8_t(b[2]));
        size = uint32_t(uint8_t(b[3])) << 24 | uint32_t(uint8_t(b[4])) << 16 | uint32_t(uint8_t(b[5])) << 8 | uint8_t(b[6]);
        return 1;
    }

    // A method frame: the head left for its size, the class and method,
    // the arguments by the writer, then finished
    inline size_t amqp_method_begin(std::string& out, uint16_t channel, uint32_t method) {
        size_t at = out.size();
        char h[11] = {char(FrameMethod), char(channel >> 8), char(channel), 0, 0, 0, 0, char(method >> 24), char(method >> 16), char(method >> 8), char(method)};
        out.append(h, 11);
        return at;
    }

    inline void amqp_frame_end(std::string& out, size_t at) {
        uint32_t size = uint32_t(out.size() - at - 7);
        out[at + 3] = char(size >> 24);
        out[at + 4] = char(size >> 16);
        out[at + 5] = char(size >> 8);
        out[at + 6] = char(size);
        out += char(FrameEnd);
    }

    // A message's content after its method: the header frame, then body
    // frames of at most frame_max - 8 bytes each
    inline void amqp_content(std::string& out, uint16_t channel, const amqp::properties& p, std::string_view body, uint32_t frame_max) {
        size_t at = out.size();
        char h[7] = {char(FrameHeader), char(channel >> 8), char(channel), 0, 0, 0, 0};
        out.append(h, 7);
        AmqpWriter w(out);
        w.u16(60);
        w.u16(0);
        w.u64(body.size());
        w.properties(p);
        amqp_frame_end(out, at);
        size_t chunk = (frame_max ? frame_max : 131072) - 8;
        for (size_t off = 0; off < body.size(); off += chunk) {
            size_t n = body.size() - off < chunk ? body.size() - off : chunk;
            size_t b = out.size();
            char bh[7] = {char(FrameBody), char(channel >> 8), char(channel), 0, 0, 0, 0};
            out.append(bh, 7);
            out.append(body.data() + off, n);
            amqp_frame_end(out, b);
        }
    }

    inline void amqp_heartbeat(std::string& out) {
        static constexpr char hb[8] = {char(FrameHeartbeat), 0, 0, 0, 0, 0, 0, char(FrameEnd)};
        out.append(hb, 8);
    }
}
