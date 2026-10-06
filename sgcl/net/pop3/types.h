//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"

#include <cstddef>
#include <cstdint>

// What the client and the server of POP3 share
namespace sgcl::net::pop3 {
    // How the connection is protected: TLS from the first byte (pop3s://,
    // port 995, RFC 8314), STLS required before the credentials go (RFC
    // 2595), or none (a test's loopback); automatic is TLS for pop3:// and
    // port 995, STLS for the rest
    enum class security : uint8_t { automatic, tls, starttls, none };

    // How a client logs in: automatic is SASL PLAIN (RFC 5034) when the
    // server offers it, else USER and PASS; APOP (RFC 1939 §7, a digest of
    // the greeting's timestamp and the password) only when asked
    enum class mechanism : uint8_t { automatic, plain, user, apop };

    // A message of the maildrop, as LIST and UIDL give it: its number in
    // the session, its size in bytes, its unique id ("" when the server
    // has no UIDL)
    struct message_info {
        uint32_t number = 0;
        uint64_t size = 0;
        string uid;

        friend bool operator==(const message_info&, const message_info&) noexcept = default;
    };

    // STAT: the messages of the maildrop not marked deleted, and their
    // size in bytes
    struct mailbox_status {
        size_t messages = 0;
        uint64_t size = 0;

        friend bool operator==(const mailbox_status&, const mailbox_status&) noexcept = default;
    };
}
