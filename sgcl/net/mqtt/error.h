//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../io/error.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace sgcl::net::mqtt {
    // The failures of MQTT that neither errno nor io names: a refusal of
    // the other side with its reason code (MQTT 5 §2.4; 3.1.1's CONNACK
    // return codes taken as their MQTT 5 equivalents), kept in the error's
    // path and read back by reason_of; a packet that breaks the
    // specification or comes out of place; a limit of either side
    enum class errc {
        refused = 1,          // CONNACK with a failure
        publish_refused,      // PUBACK, PUBREC or PUBCOMP with a failure
        subscribe_refused,    // SUBACK or UNSUBACK with a failure for every filter given
        disconnected,         // the other side sent DISCONNECT
        malformed_packet,     // a packet that breaks the specification
        protocol_error,       // a packet out of place
        packet_too_large,     // past the maximum packet size
        quota_exceeded,       // past the receive maximum: too many publications in flight
        topic_invalid         // a topic or a filter that may not be used
    };

    namespace detail {
        class MqttCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "mqtt";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::refused: return "the server refused the connection";
                    case errc::publish_refused: return "the publication was refused";
                    case errc::subscribe_refused: return "the subscription was refused";
                    case errc::disconnected: return "disconnected by the peer";
                    case errc::malformed_packet: return "malformed MQTT packet";
                    case errc::protocol_error: return "MQTT protocol error";
                    case errc::packet_too_large: return "MQTT packet too large";
                    case errc::quota_exceeded: return "MQTT quota exceeded";
                    case errc::topic_invalid: return "invalid MQTT topic";
                }
                return "unknown mqtt error";
            }
        };
    }

    // The category of errc, named "mqtt"
    inline const std::error_category& category() noexcept {
        static const detail::MqttCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::mqtt::errc> : std::true_type {};

namespace sgcl::net::mqtt {
    namespace detail {
        // The text of a reason code (MQTT 5 §2.4)
        inline const char* reason_text(uint8_t r) noexcept {
            switch (r) {
                case 0x00: return "Success";
                case 0x01: return "Granted QoS 1";
                case 0x02: return "Granted QoS 2";
                case 0x04: return "Disconnect with Will Message";
                case 0x10: return "No matching subscribers";
                case 0x11: return "No subscription existed";
                case 0x18: return "Continue authentication";
                case 0x19: return "Re-authenticate";
                case 0x80: return "Unspecified error";
                case 0x81: return "Malformed Packet";
                case 0x82: return "Protocol Error";
                case 0x83: return "Implementation specific error";
                case 0x84: return "Unsupported Protocol Version";
                case 0x85: return "Client Identifier not valid";
                case 0x86: return "Bad User Name or Password";
                case 0x87: return "Not authorized";
                case 0x88: return "Server unavailable";
                case 0x89: return "Server busy";
                case 0x8A: return "Banned";
                case 0x8B: return "Server shutting down";
                case 0x8C: return "Bad authentication method";
                case 0x8D: return "Keep Alive timeout";
                case 0x8E: return "Session taken over";
                case 0x8F: return "Topic Filter invalid";
                case 0x90: return "Topic Name invalid";
                case 0x91: return "Packet Identifier in use";
                case 0x92: return "Packet Identifier not found";
                case 0x93: return "Receive Maximum exceeded";
                case 0x94: return "Topic Alias invalid";
                case 0x95: return "Packet too large";
                case 0x96: return "Message rate too high";
                case 0x97: return "Quota exceeded";
                case 0x98: return "Administrative action";
                case 0x99: return "Payload format invalid";
                case 0x9A: return "Retain not supported";
                case 0x9B: return "QoS not supported";
                case 0x9C: return "Use another server";
                case 0x9D: return "Server moved";
                case 0x9E: return "Shared Subscriptions not supported";
                case 0x9F: return "Connection rate exceeded";
                case 0xA0: return "Maximum connect time";
                case 0xA1: return "Subscription Identifiers not supported";
                case 0xA2: return "Wildcard Subscriptions not supported";
            }
            return "Unknown reason";
        }

        // An error of a reason code: its path "0x87 Not authorized", and the
        // server's reason string after it when there is one
        inline io::error mqtt_error(errc e, const string& op, uint8_t reason, std::string_view text = {}) noexcept {
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string p = "0x";
            p += hex[reason >> 4];
            p += hex[reason & 15];
            p += ' ';
            p += reason_text(reason);
            if (!text.empty()) {
                p += ": ";
                p.append(text);
            }
            return io::error(make_error_code(e), op, string(p));
        }

        inline io::error mqtt_error(errc e, const string& op, const string& what = {}) noexcept {
            return io::error(make_error_code(e), op, what);
        }
    }

    // The reason code an error of this category carries ("0x87 Not
    // authorized" in its path); nullopt for any other error
    inline optional<uint8_t> reason_of(const io::error& e) noexcept {
        if (e.code().category() != category()) {
            return nullopt;
        }
        std::string_view p = e.path().view();
        if (p.size() < 4 || p[0] != '0' || p[1] != 'x') {
            return nullopt;
        }
        auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            }
            return -1;
        };
        int hi = digit(p[2]), lo = digit(p[3]);
        if (hi < 0 || lo < 0) {
            return nullopt;
        }
        return uint8_t(hi * 16 + lo);
    }
}
