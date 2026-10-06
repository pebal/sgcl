//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "codec.h"
#include "../types.h"
#include "../../../core/aliases.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"

#include <string>
#include <string_view>

// An application message and the PUBLISH that carries it, both ways
namespace sgcl::net::mqtt::detail {
    inline uint32_t mqtt_seconds(duration d) noexcept {
        int64_t s = d.milliseconds() / 1000;
        return s <= 0 ? 0 : s > int64_t(UINT32_MAX) ? UINT32_MAX : uint32_t(s);
    }

    // The properties of a message (PUBLISH's and a will's)
    inline void mqtt_props_of(const message& m, MqttProps& p) {
        if (m.utf8) {
            p.payload_format = 1;
        }
        if (uint32_t e = mqtt_seconds(m.expiry)) {
            p.message_expiry = e;
        }
        if (!m.content_type.empty()) {
            p.content_type = std::string(m.content_type.view());
            p.has_content_type = true;
        }
        if (!m.response_topic.empty()) {
            p.response_topic = std::string(m.response_topic.view());
            p.has_response_topic = true;
        }
        if (!m.correlation_data.empty()) {
            p.correlation_data.assign(reinterpret_cast<const char*>(m.correlation_data.data()), m.correlation_data.size());
            p.has_correlation_data = true;
        }
        for (auto& [k, v] : m.user_properties) {
            p.user.emplace_back(std::string(k.view()), std::string(v.view()));
        }
    }

    inline MqttPacket mqtt_publish_of(const message& m) {
        MqttPacket p;
        p.type = packet::publish;
        p.qos = uint8_t(m.qos);
        p.retain = m.retain;
        p.topic = std::string(m.topic.view());
        p.payload.assign(reinterpret_cast<const char*>(m.payload.data()), m.payload.size());
        mqtt_props_of(m, p.props);
        return p;
    }

    // The message a PUBLISH (or a will) carries; the topic given apart (an alias resolved)
    inline message mqtt_message_of(std::string_view topic, std::string_view payload, uint8_t qos, bool retain, const MqttProps& p) {
        message m;
        m.topic = string(topic);
        m.payload = vector<byte>(reinterpret_cast<const byte*>(payload.data()), reinterpret_cast<const byte*>(payload.data()) + payload.size());
        m.qos = mqtt::qos(qos);
        m.retain = retain;
        m.utf8 = p.payload_format && *p.payload_format == 1;
        if (p.message_expiry) {
            m.expiry = std::chrono::seconds(*p.message_expiry);
        }
        if (p.has_content_type) {
            m.content_type = string(p.content_type);
        }
        if (p.has_response_topic) {
            m.response_topic = string(p.response_topic);
        }
        if (p.has_correlation_data) {
            m.correlation_data = vector<byte>(reinterpret_cast<const byte*>(p.correlation_data.data()),
                                              reinterpret_cast<const byte*>(p.correlation_data.data()) + p.correlation_data.size());
        }
        for (auto& [k, v] : p.user) {
            m.user_properties.push_back({string(k), string(v)});
        }
        for (uint32_t id : p.subscription_ids) {
            m.subscription_ids.push_back(id);
        }
        return m;
    }

    // 3.1.1's CONNACK return codes (§3.2.2.3) as MQTT 5's reason codes
    inline uint8_t mqtt_v3_reason(uint8_t code) noexcept {
        switch (code) {
            case 0: return 0x00;
            case 1: return 0x84;
            case 2: return 0x85;
            case 3: return 0x88;
            case 4: return 0x86;
            case 5: return 0x87;
        }
        return 0x80;
    }

    // MQTT 5's reason codes of a refused CONNECT as 3.1.1's return codes
    inline uint8_t mqtt_v3_code(uint8_t reason) noexcept {
        switch (reason) {
            case 0x00: return 0;
            case 0x84: return 1;
            case 0x85: return 2;
            case 0x86: return 4;
            case 0x87: return 5;
        }
        return 3;
    }
}
