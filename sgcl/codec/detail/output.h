//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/slice.h"
#include "../../core/vector.h"
#include "../../io/stream.h"

#include <cstddef>
#include <cstdint>

namespace sgcl::codec::detail {
    // Where an encoder's bytes go: put(p, n) false when the sink failed,
    // the error in failure

    // A managed vector, grown as appending grows it
    struct VectorSink {
        vector<byte>& out;
        optional<error> failure;   // never set

        bool put(const uint8_t* p, size_t n) {
            const auto* b = reinterpret_cast<const byte*>(p);
            out.insert(out.end(), b, b + n);
            return true;
        }
    };

    // A stream: each put one write, written whole or failed (io's contract)
    struct WriterSink {
        const io::writer& out;
        uint64_t written = 0;
        optional<error> failure;

        bool put(const uint8_t* p, size_t n) {
            auto w = out.write(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            if (!w) {
                failure = error(w.error(), written);
                return false;
            }
            written += n;
            return true;
        }
    };
}
