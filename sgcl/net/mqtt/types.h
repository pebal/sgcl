//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/duration.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstdint>
#include <string_view>

// What the client and the broker of MQTT share
namespace sgcl::net::mqtt {
    // The protocol's version: 3.1.1 (OASIS 2014, its level 4) or 5.0
    enum class version : uint8_t { v3_1_1 = 4, v5 = 5 };

    // The quality of service of a delivery (MQTT 5 §4.3)
    enum class qos : uint8_t {
        at_most_once = 0,    // fire and forget
        at_least_once = 1,   // acknowledged, may come twice
        exactly_once = 2     // a handshake of four packets, once
    };

    // Whether a subscription gets the retained messages of its filter at
    // once (MQTT 5 §3.8.3.1): always, only when the subscription is new,
    // never
    enum class retain_handling : uint8_t { send = 0, send_if_new = 1, never = 2 };

    // An application message, both ways: its topic, payload, quality of
    // service and retain flag, and MQTT 5's properties (left empty, and
    // not sent, under 3.1.1)
    struct message {
        string topic;
        vector<byte> payload;
        mqtt::qos qos = mqtt::qos::at_most_once;
        bool retain = false;
        duration expiry = {};                         // Message Expiry Interval, whole seconds; zero: none
        bool utf8 = false;                            // Payload Format Indicator: the payload is UTF-8 text
        string content_type;
        string response_topic;
        vector<byte> correlation_data;
        vector<pair<string, string>> user_properties;
        vector<uint32_t> subscription_ids;            // what a message received matched (MQTT 5 §3.3.2.3.8)

        message() = default;

        // A message of a text payload
        message(const string& topic, const string& payload, mqtt::qos q = mqtt::qos::at_most_once, bool retain = false)
        : topic(topic)
        , payload(reinterpret_cast<const byte*>(payload.data()), reinterpret_cast<const byte*>(payload.data()) + payload.size())
        , qos(q)
        , retain(retain) {
        }

        // The payload as text
        string text() const {
            return string(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
        }

        friend bool operator==(const message&, const message&) noexcept = default;
    };

    // A subscription: a filter ("sensors/+/temp", "jobs/#", "$share/workers/jobs/#")
    // and how its messages are delivered
    struct subscription {
        string filter;
        mqtt::qos qos = mqtt::qos::at_most_once;        // the most the subscriber takes
        bool no_local = false;                          // not its own publications (MQTT 5)
        bool retain_as_published = false;               // the retain flag kept as published (MQTT 5)
        mqtt::retain_handling retain_handling = mqtt::retain_handling::send;
        uint32_t identifier = 0;                        // the Subscription Identifier its messages carry (MQTT 5); 0: none

        friend bool operator==(const subscription&, const subscription&) noexcept = default;
    };
}
