//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <string_view>

namespace sgcl::net::nats {
    // A message: its subject, the subject a reply goes to, its data, and
    // the headers of NATS/1.0 (HPUB, HMSG) with a status when the server
    // sets one ("503" no responders)
    struct message {
        string subject;
        string reply;                           // empty: no reply asked for
        string data;                            // bytes in a string
        vector<pair<string, string>> headers;   // in their order, a name given more than once kept so
        int status = 0;                         // the status of the header block; 0 for none
        string description;                     // the status's text ("No Responders")

        message() = default;

        message(const string& subject, const string& data)
        : subject(subject), data(data) {
        }

        // The first value of a header, its name compared without case; ""
        // for none
        string header(const string& name) const {
            for (auto& [n, v] : headers) {
                if (n.size() == name.size()) {
                    bool same = true;
                    for (size_t i = 0; i < n.size() && same; ++i) {
                        char a = n.view()[i], b = name.view()[i];
                        a = char(a + (unsigned(a - 'A') < 26u) * 32);
                        b = char(b + (unsigned(b - 'A') < 26u) * 32);
                        same = a == b;
                    }
                    if (same) {
                        return v;
                    }
                }
            }
            return string();
        }

        friend bool operator==(const message&, const message&) noexcept = default;
    };
}
