//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::mqtt's codec: the variable byte integer at its boundaries (MQTT 5
// §1.5.5's table), every packet written and read back under both versions,
// the bytes of packets the specification shows, what is malformed (the
// reserved flags, a fifth length byte, UTF-8, properties out of place or
// repeated), and the topics: names, filters and §4.7's matching examples.
#include "helpers.h"

using namespace mqtt_test;
namespace d = sgcl::net::mqtt::detail;

namespace {
    d::MqttPacket roundtrip(const d::MqttPacket& p, uint8_t level) {
        std::string bytes = d::mqtt_encode(p, level);
        size_t header = 0, total = 0;
        EXPECT_EQ(d::mqtt_frame(bytes, header, total), 1);
        EXPECT_EQ(total, bytes.size());
        d::MqttPacket out;
        uint8_t reason = 0;
        EXPECT_TRUE(d::mqtt_decode(uint8_t(bytes[0]), std::string_view(bytes).substr(header), level, out, reason)) << int(p.type) << " " << int(reason);
        return out;
    }

    std::string hex(std::string_view s) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string out;
        for (unsigned char c : s) {
            out += digits[c >> 4];
            out += digits[c & 15];
        }
        return out;
    }

    bool decodes(std::string_view bytes, uint8_t level = 5, uint8_t* why = nullptr) {
        size_t header = 0, total = 0;
        if (d::mqtt_frame(bytes, header, total) != 1 || total != bytes.size()) {
            return false;
        }
        d::MqttPacket p;
        uint8_t reason = 0;
        bool ok = d::mqtt_decode(uint8_t(bytes[0]), bytes.substr(header), level, p, reason);
        if (why) {
            *why = reason;
        }
        return ok;
    }
}

TEST(MqttCodec, VariableByteInteger) {
    for (uint32_t v : {0u, 127u, 128u, 16383u, 16384u, 2097151u, 2097152u, 268435455u}) {
        d::MqttWriter w;
        w.varint(v);
        EXPECT_EQ(w.out.size(), d::mqtt_varint_size(v)) << v;
        d::MqttReader r{w.out};
        EXPECT_EQ(r.varint(), v);
        EXPECT_FALSE(r.bad);
    }
    // §1.5.5: 127 is 0x7F, 128 is 0x80 0x01, 16383 is 0xFF 0x7F
    d::MqttWriter w;
    w.varint(128);
    EXPECT_EQ(hex(w.out), "8001");
    std::string five("\xff\xff\xff\xff\x01", 5);
    d::MqttReader r{five};
    r.varint();
    EXPECT_TRUE(r.bad);
    size_t header = 0, total = 0;
    EXPECT_EQ(d::mqtt_frame(std::string("\x30\xff\xff\xff\xff\x01", 6), header, total), -1);
    EXPECT_EQ(d::mqtt_frame(std::string("\x30\xff", 2), header, total), 0);
}

TEST(MqttCodec, KnownBytes) {
    // PINGREQ, PINGRESP, a 3.1.1 DISCONNECT (§3.12, §3.13, §3.14)
    d::MqttPacket ping;
    ping.type = d::packet::pingreq;
    EXPECT_EQ(hex(d::mqtt_encode(ping, 5)), "c000");
    ping.type = d::packet::pingresp;
    EXPECT_EQ(hex(d::mqtt_encode(ping, 5)), "d000");
    ping.type = d::packet::disconnect;
    EXPECT_EQ(hex(d::mqtt_encode(ping, 4)), "e000");
    // a 3.1.1 CONNECT of client id "c", keep alive 60, clean session
    d::MqttPacket c;
    c.type = d::packet::connect;
    c.clean_start = true;
    c.keep_alive = 60;
    c.client_id = "c";
    EXPECT_EQ(hex(d::mqtt_encode(c, 4)), "100d00044d5154540402003c000163");
    // the same under 5: level 5 and an empty property length
    EXPECT_EQ(hex(d::mqtt_encode(c, 5)), "100e00044d5154540502003c00000163");
    // a PUBLISH of QoS 1 (§3.3): topic "a/b", id 10, payload "hi"
    d::MqttPacket p;
    p.type = d::packet::publish;
    p.qos = 1;
    p.topic = "a/b";
    p.id = 10;
    p.payload = "hi";
    EXPECT_EQ(hex(d::mqtt_encode(p, 4)), "3209000361" "2f62000a6869");
    EXPECT_EQ(hex(d::mqtt_encode(p, 5)), "320a0003612f62000a006869");
}

TEST(MqttCodec, EveryPacketBothVersions) {
    for (uint8_t level : {uint8_t(4), uint8_t(5)}) {
        d::MqttPacket c;
        c.type = d::packet::connect;
        c.clean_start = true;
        c.keep_alive = 30;
        c.client_id = "client-1";
        c.has_will = true;
        c.will_qos = 1;
        c.will_retain = true;
        c.will_topic = "last/will";
        c.will_payload = "gone";
        c.has_user = true;
        c.user = "u";
        c.has_password = true;
        c.password = std::string("p\0w", 3);
        if (level == 5) {
            c.props.session_expiry = 300;
            c.props.receive_maximum = 10;
            c.props.topic_alias_maximum = 5;
            c.props.user.emplace_back("k", "v");
            c.will_props.will_delay = 7;
            c.will_props.content_type = "text/plain";
            c.will_props.has_content_type = true;
        }
        auto rc = roundtrip(c, level);
        EXPECT_EQ(rc.client_id, "client-1");
        EXPECT_EQ(rc.level, level);
        EXPECT_TRUE(rc.has_will);
        EXPECT_EQ(rc.will_qos, 1);
        EXPECT_TRUE(rc.will_retain);
        EXPECT_EQ(rc.will_topic, "last/will");
        EXPECT_EQ(rc.password, std::string("p\0w", 3));
        if (level == 5) {
            EXPECT_EQ(*rc.props.session_expiry, 300u);
            EXPECT_EQ(*rc.will_props.will_delay, 7u);
            EXPECT_EQ(rc.will_props.content_type, "text/plain");
            ASSERT_EQ(rc.props.user.size(), 1u);
        }
        d::MqttPacket a;
        a.type = d::packet::connack;
        a.session_present = true;
        a.reason = level == 5 ? 0x87 : 5;
        if (level == 5) {
            a.props.assigned_client_id = "x";
            a.props.has_assigned_client_id = true;
            a.props.maximum_qos = 1;
        }
        auto ra = roundtrip(a, level);
        EXPECT_TRUE(ra.session_present);
        EXPECT_EQ(ra.reason, a.reason);
        d::MqttPacket p;
        p.type = d::packet::publish;
        p.qos = 2;
        p.dup = true;
        p.retain = true;
        p.id = 65535;
        p.topic = "a/b/c";
        p.payload = std::string(300, 'x');
        if (level == 5) {
            p.props.message_expiry = 60;
            p.props.subscription_ids = {1, 268435455};
            p.props.correlation_data = std::string("\0\1", 2);
            p.props.has_correlation_data = true;
        }
        auto rp = roundtrip(p, level);
        EXPECT_EQ(rp.qos, 2);
        EXPECT_TRUE(rp.dup && rp.retain);
        EXPECT_EQ(rp.id, 65535);
        EXPECT_EQ(rp.payload.size(), 300u);
        if (level == 5) {
            EXPECT_EQ(rp.props.subscription_ids.size(), 2u);
            EXPECT_EQ(rp.props.correlation_data, std::string("\0\1", 2));
        }
        for (uint8_t t : {d::packet::puback, d::packet::pubrec, d::packet::pubrel, d::packet::pubcomp}) {
            d::MqttPacket ack;
            ack.type = t;
            ack.id = 7;
            ack.reason = level == 5 ? 0x10 : 0;
            auto r = roundtrip(ack, level);
            EXPECT_EQ(r.id, 7);
            EXPECT_EQ(r.reason, ack.reason);
        }
        d::MqttPacket s;
        s.type = d::packet::subscribe;
        s.id = 3;
        s.subs = {{"a/+", 1}, {"$share/g/b/#", 2}};
        if (level == 5) {
            s.subs[0].options |= 0x04 | 0x08 | 0x20;
            s.props.subscription_ids = {42};
        }
        auto rs = roundtrip(s, level);
        ASSERT_EQ(rs.subs.size(), 2u);
        EXPECT_EQ(rs.subs[0].options, s.subs[0].options);
        d::MqttPacket sa;
        sa.type = d::packet::suback;
        sa.id = 3;
        sa.reasons = {1, 0x80};
        EXPECT_EQ(roundtrip(sa, level).reasons, sa.reasons);
        d::MqttPacket u;
        u.type = d::packet::unsubscribe;
        u.id = 4;
        u.filters = {"a/+", "b"};
        EXPECT_EQ(roundtrip(u, level).filters, u.filters);
        d::MqttPacket ua;
        ua.type = d::packet::unsuback;
        ua.id = 4;
        if (level == 5) {
            ua.reasons = {0, 0x11};
        }
        EXPECT_EQ(roundtrip(ua, level).reasons, ua.reasons);
        d::MqttPacket dc;
        dc.type = d::packet::disconnect;
        if (level == 5) {
            dc.reason = 0x8E;
            dc.props.reason_string = "taken over";
            dc.props.has_reason_string = true;
        }
        auto rd = roundtrip(dc, level);
        EXPECT_EQ(rd.reason, dc.reason);
    }
}

TEST(MqttCodec, Malformed) {
    uint8_t why = 0;
    EXPECT_FALSE(decodes(std::string("\xc1\x00", 2)));           // PINGREQ with a flag
    EXPECT_FALSE(decodes(std::string("\x80\x03\x00\x01\x00", 5)));  // SUBSCRIBE without its 0x02
    EXPECT_FALSE(decodes(std::string("\x36\x05\x00\x01" "a" "\x00\x01", 7)));   // PUBLISH of QoS 3
    EXPECT_FALSE(decodes(std::string("\x38\x03\x00\x01" "a", 5)));   // QoS 0 with DUP
    EXPECT_FALSE(decodes(std::string("\x32\x05\x00\x01" "a" "\x00\x00", 7), 4));   // id 0
    EXPECT_FALSE(decodes(std::string("\x30\x04\x00\x02" "\xc3\x28", 6), 4, &why));   // not UTF-8
    EXPECT_EQ(why, 0x90);
    EXPECT_FALSE(decodes(std::string("\x30\x04\x00\x02" "a+", 6), 4));   // a wildcard in a topic name
    // a property of CONNACK's in a PUBLISH, a repeated one
    EXPECT_FALSE(decodes(std::string("\x30\x07\x00\x01" "a" "\x02\x24\x01" "x", 9)));
    EXPECT_FALSE(decodes(std::string("\x30\x08\x00\x01" "a" "\x04\x01\x00\x01\x00", 10)));
    // a PUBLISH that says it is UTF-8 and is not
    EXPECT_FALSE(decodes(std::string("\x30\x07\x00\x01" "a" "\x02\x01\x01" "\xff", 9), 5, &why));
    EXPECT_EQ(why, 0x99);
    // CONNECT: an unknown protocol level, reserved flag set
    EXPECT_FALSE(decodes(std::string("\x10\x0c\x00\x04MQTT\x07\x02\x00\x3c\x00\x00", 14), 5, &why));
    EXPECT_EQ(why, 0x84);
    EXPECT_FALSE(decodes(std::string("\x10\x0d\x00\x04MQTT\x04\x03\x00\x3c\x00\x01" "c", 15), 4));
    // SUBSCRIBE with no filter, options of QoS 3
    EXPECT_FALSE(decodes(std::string("\x82\x02\x00\x01", 4), 4));
    EXPECT_FALSE(decodes(std::string("\x82\x06\x00\x01\x00\x01" "a" "\x03", 8), 4));
    EXPECT_TRUE(decodes(std::string("\x82\x06\x00\x01\x00\x01" "a" "\x02", 8), 4));
    EXPECT_FALSE(decodes(std::string("\x00\x00", 2)));   // type 0
}

TEST(MqttCodec, Utf8) {
    EXPECT_TRUE(d::mqtt_utf8("ascii"));
    EXPECT_TRUE(d::mqtt_utf8("za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87"));
    EXPECT_TRUE(d::mqtt_utf8("\xf0\x9f\x98\x80"));
    EXPECT_FALSE(d::mqtt_utf8(std::string("a\0b", 3)));
    EXPECT_FALSE(d::mqtt_utf8("\xc0\x80"));       // overlong
    EXPECT_FALSE(d::mqtt_utf8("\xed\xa0\x80"));   // a surrogate
    EXPECT_FALSE(d::mqtt_utf8("\xe2\x82"));       // cut off
    EXPECT_FALSE(d::mqtt_utf8("\xf4\x90\x80\x80"));   // past U+10FFFF
}

// §4.7's examples
TEST(MqttCodec, Topics) {
    EXPECT_TRUE(d::mqtt_match("sport/tennis/player1/#", "sport/tennis/player1"));
    EXPECT_TRUE(d::mqtt_match("sport/tennis/player1/#", "sport/tennis/player1/ranking"));
    EXPECT_TRUE(d::mqtt_match("sport/tennis/player1/#", "sport/tennis/player1/score/wimbledon"));
    EXPECT_TRUE(d::mqtt_match("sport/#", "sport"));
    EXPECT_TRUE(d::mqtt_match("#", "anything/at/all"));
    EXPECT_TRUE(d::mqtt_match("sport/tennis/+", "sport/tennis/player1"));
    EXPECT_FALSE(d::mqtt_match("sport/tennis/+", "sport/tennis/player1/ranking"));
    EXPECT_FALSE(d::mqtt_match("sport/+", "sport"));
    EXPECT_TRUE(d::mqtt_match("sport/+", "sport/"));
    EXPECT_TRUE(d::mqtt_match("+/+", "/finance"));
    EXPECT_TRUE(d::mqtt_match("/+", "/finance"));
    EXPECT_FALSE(d::mqtt_match("+", "/finance"));
    EXPECT_FALSE(d::mqtt_match("#", "$SYS/monitor"));
    EXPECT_FALSE(d::mqtt_match("+/monitor/Clients", "$SYS/monitor/Clients"));
    EXPECT_TRUE(d::mqtt_match("$SYS/#", "$SYS/monitor/Clients"));
    EXPECT_TRUE(d::mqtt_match("$SYS/monitor/+", "$SYS/monitor/Clients"));
    EXPECT_TRUE(d::mqtt_match("a/b", "a/b"));
    EXPECT_FALSE(d::mqtt_match("a/b", "a/b/c"));
    EXPECT_FALSE(d::mqtt_match("a/b/c", "a/b"));
    EXPECT_TRUE(d::mqtt_filter_valid("sport/tennis/#"));
    EXPECT_TRUE(d::mqtt_filter_valid("+"));
    EXPECT_TRUE(d::mqtt_filter_valid("+/tennis/#"));
    EXPECT_TRUE(d::mqtt_filter_valid("$share/group/a/+"));
    EXPECT_FALSE(d::mqtt_filter_valid("sport/tennis#"));
    EXPECT_FALSE(d::mqtt_filter_valid("sport/tennis/#/ranking"));
    EXPECT_FALSE(d::mqtt_filter_valid("sport+"));
    EXPECT_FALSE(d::mqtt_filter_valid(""));
    EXPECT_FALSE(d::mqtt_filter_valid("$share/gr+oup/a"));
    EXPECT_FALSE(d::mqtt_filter_valid("$share//a"));
    EXPECT_FALSE(d::mqtt_filter_valid("$share/group/"));
    EXPECT_TRUE(d::mqtt_topic_valid("a/b"));
    EXPECT_TRUE(d::mqtt_topic_valid("/"));
    EXPECT_FALSE(d::mqtt_topic_valid(""));
    EXPECT_FALSE(d::mqtt_topic_valid("a/+"));
    EXPECT_FALSE(d::mqtt_topic_valid("a/#"));
    std::string_view group;
    EXPECT_EQ(d::mqtt_share("$share/g1/x/y", &group), "x/y");
    EXPECT_EQ(group, "g1");
    EXPECT_EQ(d::mqtt_share("x/y", &group), "x/y");
    EXPECT_TRUE(group.empty());
}

TEST(MqttCodec, ErrorsAndReasons) {
    auto e = d::mqtt_error(mqtt::errc::refused, "mqtt connect", 0x87, "go away");
    EXPECT_EQ(str(e.path()), "0x87 Not authorized: go away");
    EXPECT_EQ(mqtt::reason_of(e), uint8_t(0x87));
    EXPECT_FALSE(mqtt::reason_of(io::error(io::errc::closed, "x", sgcl::string())));
    EXPECT_FALSE(mqtt::reason_of(d::mqtt_error(mqtt::errc::topic_invalid, "x", sgcl::string("a/+"))));
    error_code c = mqtt::errc::packet_too_large;
    EXPECT_EQ(std::string(c.category().name()), "mqtt");
    EXPECT_EQ(c.message(), "MQTT packet too large");
    EXPECT_EQ(d::mqtt_v3_reason(5), 0x87);
    EXPECT_EQ(d::mqtt_v3_code(0x86), 4);
}
