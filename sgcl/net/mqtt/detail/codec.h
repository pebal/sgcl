//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../types.h"
#include "../../../core/aliases.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// MQTT's packets (MQTT 5.0 §2–§3, 3.1.1 §2–§3) read and written: the
// fixed header with its variable byte integer, the variable headers, the
// properties of MQTT 5 with the packets each may appear in, the payloads;
// topic names and filters checked and matched (§4.7)
namespace sgcl::net::mqtt::detail {
    namespace packet {
        inline constexpr uint8_t connect = 1;
        inline constexpr uint8_t connack = 2;
        inline constexpr uint8_t publish = 3;
        inline constexpr uint8_t puback = 4;
        inline constexpr uint8_t pubrec = 5;
        inline constexpr uint8_t pubrel = 6;
        inline constexpr uint8_t pubcomp = 7;
        inline constexpr uint8_t subscribe = 8;
        inline constexpr uint8_t suback = 9;
        inline constexpr uint8_t unsubscribe = 10;
        inline constexpr uint8_t unsuback = 11;
        inline constexpr uint8_t pingreq = 12;
        inline constexpr uint8_t pingresp = 13;
        inline constexpr uint8_t disconnect = 14;
        inline constexpr uint8_t auth = 15;
    }

    // The properties of MQTT 5 (§2.2.2.2), each as its packets carry it
    struct MqttProps {
        optional<uint8_t> payload_format;           // 0x01
        optional<uint32_t> message_expiry;          // 0x02
        std::string content_type;                   // 0x03
        bool has_content_type = false;
        std::string response_topic;                 // 0x08
        bool has_response_topic = false;
        std::string correlation_data;               // 0x09
        bool has_correlation_data = false;
        std::vector<uint32_t> subscription_ids;     // 0x0B
        optional<uint32_t> session_expiry;          // 0x11
        std::string assigned_client_id;             // 0x12
        bool has_assigned_client_id = false;
        optional<uint16_t> server_keep_alive;       // 0x13
        std::string auth_method;                    // 0x15
        bool has_auth_method = false;
        std::string auth_data;                      // 0x16
        bool has_auth_data = false;
        optional<uint8_t> request_problem_info;     // 0x17
        optional<uint32_t> will_delay;              // 0x18
        optional<uint8_t> request_response_info;    // 0x19
        std::string response_info;                  // 0x1A
        bool has_response_info = false;
        std::string server_reference;               // 0x1C
        bool has_server_reference = false;
        std::string reason_string;                  // 0x1F
        bool has_reason_string = false;
        optional<uint16_t> receive_maximum;         // 0x21
        optional<uint16_t> topic_alias_maximum;     // 0x22
        optional<uint16_t> topic_alias;             // 0x23
        optional<uint8_t> maximum_qos;              // 0x24
        optional<uint8_t> retain_available;         // 0x25
        std::vector<std::pair<std::string, std::string>> user;   // 0x26
        optional<uint32_t> maximum_packet_size;     // 0x27
        optional<uint8_t> wildcard_available;       // 0x28
        optional<uint8_t> subscription_id_available; // 0x29
        optional<uint8_t> shared_available;         // 0x2A
    };

    // A subscription of a SUBSCRIBE, as it is on the wire
    struct MqttSub {
        std::string filter;
        uint8_t options = 0;   // qos | no_local << 2 | retain_as_published << 3 | retain_handling << 4
    };

    // One packet of any type, the fields of its type filled
    struct MqttPacket {
        uint8_t type = 0;
        uint8_t flags = 0;
        // CONNECT
        std::string protocol_name;
        uint8_t level = 5;
        bool clean_start = false;
        uint16_t keep_alive = 0;
        std::string client_id;
        bool has_will = false;
        uint8_t will_qos = 0;
        bool will_retain = false;
        MqttProps will_props;
        std::string will_topic;
        std::string will_payload;
        bool has_user = false;
        std::string user;
        bool has_password = false;
        std::string password;
        // CONNACK
        bool session_present = false;
        // the reason code of CONNACK, PUBACK, PUBREC, PUBREL, PUBCOMP, DISCONNECT, AUTH
        uint8_t reason = 0;
        // PUBLISH
        bool dup = false;
        uint8_t qos = 0;
        bool retain = false;
        std::string topic;
        std::string payload;
        // the packet identifier of PUBLISH (QoS > 0), the acknowledgements, SUBSCRIBE, UNSUBSCRIBE and their acks
        uint16_t id = 0;
        // SUBSCRIBE
        std::vector<MqttSub> subs;
        // UNSUBSCRIBE
        std::vector<std::string> filters;
        // SUBACK, UNSUBACK
        std::vector<uint8_t> reasons;
        MqttProps props;
    };

    // ---- primitives

    struct MqttWriter {
        std::string out;

        void u8(uint8_t v) {
            out += char(v);
        }

        void u16(uint16_t v) {
            out += char(v >> 8);
            out += char(v);
        }

        void u32(uint32_t v) {
            out += char(v >> 24);
            out += char(v >> 16);
            out += char(v >> 8);
            out += char(v);
        }

        void varint(uint32_t v) {
            do {
                uint8_t b = uint8_t(v & 0x7F);
                v >>= 7;
                if (v) {
                    b |= 0x80;
                }
                out += char(b);
            } while (v);
        }

        void str(std::string_view s) {
            u16(uint16_t(s.size()));
            out.append(s);
        }
    };

    inline size_t mqtt_varint_size(uint32_t v) noexcept {
        return v < 128 ? 1 : v < 16384 ? 2 : v < 2097152 ? 3 : 4;
    }

    struct MqttReader {
        std::string_view in;
        size_t at = 0;
        bool bad = false;

        size_t left() const noexcept {
            return in.size() - at;
        }

        uint8_t u8() noexcept {
            if (left() < 1) {
                bad = true;
                return 0;
            }
            return uint8_t(in[at++]);
        }

        uint16_t u16() noexcept {
            if (left() < 2) {
                bad = true;
                at = in.size();
                return 0;
            }
            uint16_t v = uint16_t(uint8_t(in[at]) << 8 | uint8_t(in[at + 1]));
            at += 2;
            return v;
        }

        uint32_t u32() noexcept {
            if (left() < 4) {
                bad = true;
                at = in.size();
                return 0;
            }
            uint32_t v = uint32_t(uint8_t(in[at])) << 24 | uint32_t(uint8_t(in[at + 1])) << 16 | uint32_t(uint8_t(in[at + 2])) << 8 | uint8_t(in[at + 3]);
            at += 4;
            return v;
        }

        uint32_t varint() noexcept {
            uint32_t v = 0;
            for (int i = 0; i < 4; ++i) {
                if (left() < 1) {
                    bad = true;
                    return 0;
                }
                uint8_t b = uint8_t(in[at++]);
                v |= uint32_t(b & 0x7F) << (7 * i);
                if (!(b & 0x80)) {
                    return v;
                }
            }
            bad = true;   // a fifth byte
            return 0;
        }

        std::string_view bytes(size_t n) noexcept {
            if (left() < n) {
                bad = true;
                at = in.size();
                return {};
            }
            std::string_view v = in.substr(at, n);
            at += n;
            return v;
        }

        std::string_view str() noexcept {
            uint16_t n = u16();
            return bytes(n);
        }
    };

    // Well-formed UTF-8 without U+0000 and without the surrogates (§1.5.4)
    inline bool mqtt_utf8(std::string_view s) noexcept {
        size_t i = 0;
        while (i < s.size()) {
            uint8_t c = uint8_t(s[i]);
            if (c < 0x80) {
                if (c == 0) {
                    return false;
                }
                ++i;
                continue;
            }
            size_t n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 ? 1 : 0;
            if (n == 0 || c > 0xF4 || i + n >= s.size()) {
                return false;   // a lead byte of no sequence, or a sequence cut off
            }
            uint32_t cp = c & (0x3F >> n);
            for (size_t k = 1; k <= n; ++k) {
                uint8_t d = uint8_t(s[i + k]);
                if ((d & 0xC0) != 0x80) {
                    return false;
                }
                cp = cp << 6 | (d & 0x3F);
            }
            if ((n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF)) {
                return false;
            }
            i += n + 1;
        }
        return true;
    }

    // ---- topics (§4.7)

    // A topic name to publish to: not empty, no wildcard, UTF-8
    inline bool mqtt_topic_valid(std::string_view t) noexcept {
        if (t.empty() || t.size() > 65535) {
            return false;
        }
        for (char c : t) {
            if (c == '+' || c == '#') {
                return false;
            }
        }
        return mqtt_utf8(t);
    }

    // A topic filter: "+" alone in a level, "#" alone and last; a shared
    // subscription "$share/<group>/<filter>" with a group of no wildcard
    // and no "/"
    inline bool mqtt_filter_valid(std::string_view f) noexcept {
        if (f.empty() || f.size() > 65535 || !mqtt_utf8(f)) {
            return false;
        }
        if (f.substr(0, 7) == "$share/") {
            size_t slash = f.find('/', 7);
            if (slash == std::string_view::npos || slash == 7) {
                return false;
            }
            for (char c : f.substr(7, slash - 7)) {
                if (c == '+' || c == '#') {
                    return false;
                }
            }
            f = f.substr(slash + 1);
            if (f.empty()) {
                return false;
            }
        }
        size_t start = 0;
        for (size_t i = 0; i <= f.size(); ++i) {
            if (i == f.size() || f[i] == '/') {
                std::string_view level = f.substr(start, i - start);
                if (level.find_first_of("+#") != std::string_view::npos) {
                    if (level.size() != 1) {
                        return false;
                    }
                    if (level == "#" && i != f.size()) {
                        return false;
                    }
                }
                start = i + 1;
            }
        }
        return true;
    }

    // The filter of a shared subscription without its "$share/<group>/",
    // and the group; the filter as it is and "" otherwise
    inline std::string_view mqtt_share(std::string_view f, std::string_view* group = nullptr) noexcept {
        if (f.substr(0, 7) == "$share/") {
            size_t slash = f.find('/', 7);
            if (slash != std::string_view::npos) {
                if (group) {
                    *group = f.substr(7, slash - 7);
                }
                return f.substr(slash + 1);
            }
        }
        if (group) {
            *group = std::string_view();
        }
        return f;
    }

    // Whether a topic matches a filter (§4.7.1, §4.7.2): "+" one level,
    // "#" the rest (the parent level too), a topic starting with "$" not
    // matched by a wildcard at the first level
    inline bool mqtt_match(std::string_view filter, std::string_view topic) noexcept {
        if (!topic.empty() && topic[0] == '$' && !filter.empty() && (filter[0] == '+' || filter[0] == '#')) {
            return false;
        }
        size_t f = 0, t = 0;
        for (;;) {
            size_t fe = filter.find('/', f);
            std::string_view fl = filter.substr(f, fe == std::string_view::npos ? std::string_view::npos : fe - f);
            if (fl == "#") {
                return true;
            }
            if (t > topic.size()) {
                // the topic ended at the level before: only "#" may match its parent
                return false;
            }
            size_t te = topic.find('/', t);
            std::string_view tl = topic.substr(t, te == std::string_view::npos ? std::string_view::npos : te - t);
            if (fl != "+" && fl != tl) {
                return false;
            }
            if (fe == std::string_view::npos) {
                return te == std::string_view::npos;
            }
            if (te == std::string_view::npos) {
                // the topic has no more levels: "a/#" matches "a"
                std::string_view rest = filter.substr(fe + 1);
                return rest == "#";
            }
            f = fe + 1;
            t = te + 1;
        }
    }

    // ---- properties

    // The packets a property may be in (§2.2.2.2), a bit per packet type;
    // the will's properties are CONNECT's payload, bit 0
    inline uint32_t mqtt_prop_packets(uint8_t id) noexcept {
        auto b = [](std::initializer_list<int> ts) {
            uint32_t m = 0;
            for (int t : ts) {
                m |= 1u << t;
            }
            return m;
        };
        switch (id) {
            case 0x01: case 0x02: case 0x03: case 0x08: case 0x09: return b({0, packet::publish});
            case 0x0B: return b({packet::publish, packet::subscribe});
            case 0x11: return b({packet::connect, packet::connack, packet::disconnect});
            case 0x12: case 0x13: case 0x1A: return b({packet::connack});
            case 0x15: case 0x16: return b({packet::connect, packet::connack, packet::auth});
            case 0x17: case 0x19: return b({packet::connect});
            case 0x18: return b({0});
            case 0x1C: return b({packet::connack, packet::disconnect});
            case 0x1F: return b({packet::connack, packet::puback, packet::pubrec, packet::pubrel, packet::pubcomp, packet::suback, packet::unsuback, packet::disconnect, packet::auth});
            case 0x21: case 0x22: case 0x27: return b({packet::connect, packet::connack});
            case 0x23: return b({packet::publish});
            case 0x24: case 0x25: case 0x28: case 0x29: case 0x2A: return b({packet::connack});
            case 0x26: return 0xFFFFu;
        }
        return 0;
    }

    // The properties of a packet of type `of` (0 for a will); false for a
    // property unknown, out of place, repeated or of a value out of range
    inline bool mqtt_read_props(MqttReader& r, uint8_t of, MqttProps& p) noexcept {
        uint32_t len = r.varint();
        if (r.bad || len > r.left()) {
            return false;
        }
        MqttReader in{r.bytes(len)};
        uint64_t seen = 0;
        while (in.left() && !in.bad) {
            uint32_t id = in.varint();
            if (in.bad || id > 0x2A || !(mqtt_prop_packets(uint8_t(id)) & (1u << of))) {
                return false;
            }
            uint64_t bit = uint64_t(1) << id;
            if ((seen & bit) && id != 0x26 && id != 0x0B) {
                return false;
            }
            seen |= bit;
            auto text = [&](std::string& out, bool& has) {
                std::string_view v = in.str();
                if (in.bad || !mqtt_utf8(v)) {
                    return false;
                }
                out = std::string(v);
                has = true;
                return true;
            };
            auto boolean = [&](optional<uint8_t>& out) {
                uint8_t v = in.u8();
                if (v > 1) {
                    return false;
                }
                out = v;
                return true;
            };
            bool ok = true;
            switch (id) {
                case 0x01: ok = boolean(p.payload_format); break;
                case 0x02: p.message_expiry = in.u32(); break;
                case 0x03: ok = text(p.content_type, p.has_content_type); break;
                case 0x08: ok = text(p.response_topic, p.has_response_topic) && mqtt_topic_valid(p.response_topic); break;
                case 0x09: {
                    std::string_view v = in.str();
                    p.correlation_data = std::string(v);
                    p.has_correlation_data = true;
                    break;
                }
                case 0x0B: {
                    uint32_t v = in.varint();
                    ok = v != 0;
                    p.subscription_ids.push_back(v);
                    break;
                }
                case 0x11: p.session_expiry = in.u32(); break;
                case 0x12: ok = text(p.assigned_client_id, p.has_assigned_client_id); break;
                case 0x13: p.server_keep_alive = in.u16(); break;
                case 0x15: ok = text(p.auth_method, p.has_auth_method); break;
                case 0x16: {
                    std::string_view v = in.str();
                    p.auth_data = std::string(v);
                    p.has_auth_data = true;
                    break;
                }
                case 0x17: ok = boolean(p.request_problem_info); break;
                case 0x18: p.will_delay = in.u32(); break;
                case 0x19: ok = boolean(p.request_response_info); break;
                case 0x1A: ok = text(p.response_info, p.has_response_info); break;
                case 0x1C: ok = text(p.server_reference, p.has_server_reference); break;
                case 0x1F: ok = text(p.reason_string, p.has_reason_string); break;
                case 0x21: p.receive_maximum = in.u16(); ok = *p.receive_maximum != 0; break;
                case 0x22: p.topic_alias_maximum = in.u16(); break;
                case 0x23: p.topic_alias = in.u16(); ok = *p.topic_alias != 0; break;
                case 0x24: ok = boolean(p.maximum_qos); break;
                case 0x25: ok = boolean(p.retain_available); break;
                case 0x26: {
                    std::string_view k = in.str();
                    std::string_view v = in.str();
                    ok = !in.bad && mqtt_utf8(k) && mqtt_utf8(v);
                    p.user.emplace_back(std::string(k), std::string(v));
                    break;
                }
                case 0x27: p.maximum_packet_size = in.u32(); ok = *p.maximum_packet_size != 0; break;
                case 0x28: ok = boolean(p.wildcard_available); break;
                case 0x29: ok = boolean(p.subscription_id_available); break;
                case 0x2A: ok = boolean(p.shared_available); break;
                default: ok = false;
            }
            if (!ok || in.bad) {
                return false;
            }
        }
        return !in.bad;
    }

    inline void mqtt_write_props(MqttWriter& w, const MqttProps& p) {
        MqttWriter b;
        auto text = [&](uint8_t id, const std::string& v) {
            b.u8(id);
            b.str(v);
        };
        if (p.payload_format) {
            b.u8(0x01);
            b.u8(*p.payload_format);
        }
        if (p.message_expiry) {
            b.u8(0x02);
            b.u32(*p.message_expiry);
        }
        if (p.has_content_type) {
            text(0x03, p.content_type);
        }
        if (p.has_response_topic) {
            text(0x08, p.response_topic);
        }
        if (p.has_correlation_data) {
            text(0x09, p.correlation_data);
        }
        for (uint32_t s : p.subscription_ids) {
            b.u8(0x0B);
            b.varint(s);
        }
        if (p.session_expiry) {
            b.u8(0x11);
            b.u32(*p.session_expiry);
        }
        if (p.has_assigned_client_id) {
            text(0x12, p.assigned_client_id);
        }
        if (p.server_keep_alive) {
            b.u8(0x13);
            b.u16(*p.server_keep_alive);
        }
        if (p.has_auth_method) {
            text(0x15, p.auth_method);
        }
        if (p.has_auth_data) {
            text(0x16, p.auth_data);
        }
        if (p.request_problem_info) {
            b.u8(0x17);
            b.u8(*p.request_problem_info);
        }
        if (p.will_delay) {
            b.u8(0x18);
            b.u32(*p.will_delay);
        }
        if (p.request_response_info) {
            b.u8(0x19);
            b.u8(*p.request_response_info);
        }
        if (p.has_response_info) {
            text(0x1A, p.response_info);
        }
        if (p.has_server_reference) {
            text(0x1C, p.server_reference);
        }
        if (p.has_reason_string) {
            text(0x1F, p.reason_string);
        }
        if (p.receive_maximum) {
            b.u8(0x21);
            b.u16(*p.receive_maximum);
        }
        if (p.topic_alias_maximum) {
            b.u8(0x22);
            b.u16(*p.topic_alias_maximum);
        }
        if (p.topic_alias) {
            b.u8(0x23);
            b.u16(*p.topic_alias);
        }
        if (p.maximum_qos) {
            b.u8(0x24);
            b.u8(*p.maximum_qos);
        }
        if (p.retain_available) {
            b.u8(0x25);
            b.u8(*p.retain_available);
        }
        for (auto& [k, v] : p.user) {
            b.u8(0x26);
            b.str(k);
            b.str(v);
        }
        if (p.maximum_packet_size) {
            b.u8(0x27);
            b.u32(*p.maximum_packet_size);
        }
        if (p.wildcard_available) {
            b.u8(0x28);
            b.u8(*p.wildcard_available);
        }
        if (p.subscription_id_available) {
            b.u8(0x29);
            b.u8(*p.subscription_id_available);
        }
        if (p.shared_available) {
            b.u8(0x2A);
            b.u8(*p.shared_available);
        }
        w.varint(uint32_t(b.out.size()));
        w.out += b.out;
    }

    // ---- framing

    // The fixed header of the packet at the buffer's front: 1 with its
    // whole size (header and body) and the header's size, 0 when it is not
    // whole yet, -1 for a remaining length of five bytes
    inline int mqtt_frame(std::string_view buf, size_t& header, size_t& total) noexcept {
        if (buf.size() < 2) {
            return 0;
        }
        uint32_t len = 0;
        for (size_t i = 1; i <= 4; ++i) {
            if (i >= buf.size()) {
                return 0;
            }
            uint8_t b = uint8_t(buf[i]);
            len |= uint32_t(b & 0x7F) << (7 * (i - 1));
            if (!(b & 0x80)) {
                header = i + 1;
                total = header + len;
                return 1;
            }
        }
        return -1;
    }

    // ---- encoding

    // The packet whole: the fixed header before the body w holds
    inline std::string mqtt_finish(uint8_t first, const MqttWriter& body) {
        MqttWriter w;
        w.out.reserve(body.out.size() + 5);
        w.u8(first);
        w.varint(uint32_t(body.out.size()));
        w.out += body.out;
        return std::move(w.out);
    }

    inline std::string mqtt_encode(const MqttPacket& p, uint8_t level) {
        const bool v5 = level >= 5;
        MqttWriter b;
        uint8_t first = uint8_t(p.type << 4);
        switch (p.type) {
            case packet::connect: {
                b.str(level >= 4 ? "MQTT" : "MQIsdp");
                b.u8(level);
                uint8_t flags = 0;
                flags |= p.clean_start ? 0x02 : 0;
                if (p.has_will) {
                    flags |= 0x04 | uint8_t(p.will_qos << 3) | (p.will_retain ? 0x20 : 0);
                }
                flags |= p.has_password ? 0x40 : 0;
                flags |= p.has_user ? 0x80 : 0;
                b.u8(flags);
                b.u16(p.keep_alive);
                if (v5) {
                    mqtt_write_props(b, p.props);
                }
                b.str(p.client_id);
                if (p.has_will) {
                    if (v5) {
                        mqtt_write_props(b, p.will_props);
                    }
                    b.str(p.will_topic);
                    b.str(p.will_payload);
                }
                if (p.has_user) {
                    b.str(p.user);
                }
                if (p.has_password) {
                    b.str(p.password);
                }
                break;
            }
            case packet::connack:
                b.u8(p.session_present ? 1 : 0);
                b.u8(p.reason);
                if (v5) {
                    mqtt_write_props(b, p.props);
                }
                break;
            case packet::publish:
                first |= uint8_t((p.dup ? 0x08 : 0) | (p.qos << 1) | (p.retain ? 1 : 0));
                b.str(p.topic);
                if (p.qos > 0) {
                    b.u16(p.id);
                }
                if (v5) {
                    mqtt_write_props(b, p.props);
                }
                b.out += p.payload;
                break;
            case packet::puback:
            case packet::pubrec:
            case packet::pubrel:
            case packet::pubcomp:
                if (p.type == packet::pubrel) {
                    first |= 0x02;
                }
                b.u16(p.id);
                if (v5 && (p.reason != 0 || p.props.has_reason_string || !p.props.user.empty())) {
                    b.u8(p.reason);
                    if (p.props.has_reason_string || !p.props.user.empty()) {
                        mqtt_write_props(b, p.props);
                    }
                }
                break;
            case packet::subscribe:
                first |= 0x02;
                b.u16(p.id);
                if (v5) {
                    mqtt_write_props(b, p.props);
                }
                for (auto& s : p.subs) {
                    b.str(s.filter);
                    b.u8(v5 ? s.options : uint8_t(s.options & 3));
                }
                break;
            case packet::suback:
            case packet::unsuback:
                b.u16(p.id);
                if (v5) {
                    mqtt_write_props(b, p.props);
                }
                if (v5 || p.type == packet::suback) {
                    for (uint8_t r : p.reasons) {
                        b.u8(r);
                    }
                }
                break;
            case packet::unsubscribe:
                first |= 0x02;
                b.u16(p.id);
                if (v5) {
                    mqtt_write_props(b, p.props);
                }
                for (auto& f : p.filters) {
                    b.str(f);
                }
                break;
            case packet::pingreq:
            case packet::pingresp:
                break;
            case packet::disconnect:
            case packet::auth:
                if (v5 && (p.reason != 0 || p.props.has_reason_string || p.props.session_expiry || p.props.has_server_reference || !p.props.user.empty())) {
                    b.u8(p.reason);
                    mqtt_write_props(b, p.props);
                }
                break;
        }
        return mqtt_finish(first, b);
    }

    // ---- decoding

    // A packet from its first byte and its body; false for one that breaks
    // the specification, the reason code to answer it with in `reason`
    // (0x81 malformed, 0x82 protocol error)
    inline bool mqtt_decode(uint8_t first, std::string_view body, uint8_t level, MqttPacket& p, uint8_t& reason) noexcept {
        p.type = uint8_t(first >> 4);
        p.flags = uint8_t(first & 0x0F);
        const bool v5 = level >= 5;
        reason = 0x81;
        MqttReader r{body};
        // the reserved flags of every packet but PUBLISH (§2.1.3)
        uint8_t want = (p.type == packet::pubrel || p.type == packet::subscribe || p.type == packet::unsubscribe) ? 2 : 0;
        if (p.type != packet::publish && p.flags != want) {
            return false;
        }
        switch (p.type) {
            case packet::connect: {
                p.protocol_name = std::string(r.str());
                p.level = r.u8();
                uint8_t flags = r.u8();
                p.keep_alive = r.u16();
                if (r.bad || (flags & 0x01)) {
                    return false;
                }
                if ((p.protocol_name != "MQTT" && p.protocol_name != "MQIsdp") || (p.level != 4 && p.level != 5 && p.level != 3)) {
                    reason = 0x84;   // unsupported protocol version
                    return false;
                }
                const bool cv5 = p.level >= 5;
                p.clean_start = flags & 0x02;
                p.has_will = flags & 0x04;
                p.will_qos = uint8_t((flags >> 3) & 3);
                p.will_retain = flags & 0x20;
                p.has_password = flags & 0x40;
                p.has_user = flags & 0x80;
                if ((!p.has_will && (p.will_qos || p.will_retain)) || p.will_qos > 2 || (!cv5 && p.has_password && !p.has_user)) {
                    return false;
                }
                if (cv5 && !mqtt_read_props(r, packet::connect, p.props)) {
                    return false;
                }
                std::string_view id = r.str();
                if (r.bad || !mqtt_utf8(id)) {
                    return false;
                }
                p.client_id = std::string(id);
                if (p.has_will) {
                    if (cv5 && !mqtt_read_props(r, 0, p.will_props)) {
                        return false;
                    }
                    std::string_view t = r.str();
                    std::string_view pl = r.str();
                    if (r.bad || !mqtt_topic_valid(t)) {
                        return false;
                    }
                    p.will_topic = std::string(t);
                    p.will_payload = std::string(pl);
                }
                if (p.has_user) {
                    std::string_view u = r.str();
                    if (r.bad || !mqtt_utf8(u)) {
                        return false;
                    }
                    p.user = std::string(u);
                }
                if (p.has_password) {
                    p.password = std::string(r.str());
                }
                return !r.bad && r.left() == 0;
            }
            case packet::connack: {
                uint8_t ack = r.u8();
                p.session_present = ack & 1;
                p.reason = r.u8();
                if (r.bad || (ack & 0xFE)) {
                    return false;
                }
                if (v5 && !mqtt_read_props(r, packet::connack, p.props)) {
                    return false;
                }
                return r.left() == 0;
            }
            case packet::publish: {
                p.dup = p.flags & 0x08;
                p.qos = uint8_t((p.flags >> 1) & 3);
                p.retain = p.flags & 1;
                if (p.qos == 3 || (p.qos == 0 && p.dup)) {
                    return false;
                }
                std::string_view t = r.str();
                if (r.bad || !mqtt_utf8(t) || t.find_first_of("+#") != std::string_view::npos) {
                    reason = 0x90;   // topic name invalid
                    return false;
                }
                p.topic = std::string(t);
                if (p.qos > 0) {
                    p.id = r.u16();
                    if (r.bad || p.id == 0) {
                        return false;
                    }
                }
                if (v5 && !mqtt_read_props(r, packet::publish, p.props)) {
                    return false;
                }
                if (p.topic.empty() && !(v5 && p.props.topic_alias)) {
                    reason = 0x82;
                    return false;
                }
                p.payload = std::string(r.bytes(r.left()));
                if (p.props.payload_format && *p.props.payload_format == 1 && !mqtt_utf8(p.payload)) {
                    reason = 0x99;   // payload format invalid
                    return false;
                }
                return !r.bad;
            }
            case packet::puback:
            case packet::pubrec:
            case packet::pubrel:
            case packet::pubcomp: {
                p.id = r.u16();
                if (r.bad || p.id == 0) {
                    return false;
                }
                if (v5 && r.left()) {
                    p.reason = r.u8();
                    if (r.left() && !mqtt_read_props(r, p.type, p.props)) {
                        return false;
                    }
                }
                return !r.bad && r.left() == 0;
            }
            case packet::subscribe: {
                p.id = r.u16();
                if (r.bad || p.id == 0) {
                    return false;
                }
                if (v5 && !mqtt_read_props(r, packet::subscribe, p.props)) {
                    return false;
                }
                if (p.props.subscription_ids.size() > 1) {
                    reason = 0x82;
                    return false;
                }
                while (r.left()) {
                    std::string_view f = r.str();
                    uint8_t o = r.u8();
                    if (r.bad || (o & 3) == 3 || (o & 0xC0) || (!v5 && (o & 0xFC)) || ((o >> 4) & 3) == 3) {
                        return false;
                    }
                    p.subs.push_back(MqttSub{std::string(f), o});
                }
                if (p.subs.empty()) {
                    reason = 0x82;
                    return false;
                }
                return !r.bad;
            }
            case packet::suback:
            case packet::unsuback: {
                p.id = r.u16();
                if (r.bad) {
                    return false;
                }
                if (v5 && !mqtt_read_props(r, p.type, p.props)) {
                    return false;
                }
                if (p.type == packet::unsuback && !v5) {
                    return r.left() == 0;   // 3.1.1's UNSUBACK is its identifier alone
                }
                while (r.left()) {
                    p.reasons.push_back(r.u8());
                }
                return !r.bad && !p.reasons.empty();
            }
            case packet::unsubscribe: {
                p.id = r.u16();
                if (r.bad || p.id == 0) {
                    return false;
                }
                if (v5 && !mqtt_read_props(r, packet::unsubscribe, p.props)) {
                    return false;
                }
                while (r.left()) {
                    std::string_view f = r.str();
                    if (r.bad) {
                        return false;
                    }
                    p.filters.push_back(std::string(f));
                }
                if (p.filters.empty()) {
                    reason = 0x82;
                    return false;
                }
                return true;
            }
            case packet::pingreq:
            case packet::pingresp:
                return r.left() == 0;
            case packet::disconnect:
            case packet::auth:
                if (!v5) {
                    return p.type == packet::disconnect && r.left() == 0;
                }
                if (r.left()) {
                    p.reason = r.u8();
                    if (r.left() && !mqtt_read_props(r, p.type, p.props)) {
                        return false;
                    }
                }
                return !r.bad && r.left() == 0;
        }
        return false;   // type 0
    }
}
