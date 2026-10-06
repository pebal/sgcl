//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "json_text.h"
#include "text_scan.h"

#include <cstddef>
#include <cstdint>

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    // Whether the bytes are UTF-8, each sequence well-formed (no overlong
    // form, no surrogate, nothing past U+10FFFF), as the text strings of
    // CBOR, MessagePack, YAML and TOML must be. ASCII runs are passed
    // eight bytes at a time in a word (a NEON loop of sixteen measured no
    // faster: 24 GB/s either way, the memory's speed); a byte of 0x80 and
    // above is weighed sequence by sequence
    inline bool utf8_text_valid(const char* p, const char* end) noexcept {
        for (;;) {
            while (end - p >= 8) {
                uint64_t m = load_word(p) & Highs;
                if (m) {
                    p = first_flagged(p, m);
                    break;
                }
                p += 8;
            }
            while (p != end && uint8_t(*p) < 0x80) {
                ++p;
            }
            if (p == end) {
                return true;
            }
            // the sequences up to the next ASCII byte
            while (p != end && uint8_t(*p) >= 0x80) {
                int n = utf8_sequence(p, end);
                if (n <= 0) {
                    return false;
                }
                p += n;
            }
        }
    }
}
