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
#include <utility>

namespace sgcl::codec::detail {
    // Where an encoder's bytes go: put(p, n) false when the sink failed,
    // the error in failure (where an encoder also puts its refusal of an
    // image its format cannot hold)

    // A managed vector, grown as appending grows it
    struct VectorSink {
        vector<byte>& out;
        optional<error> failure;   // the encoder's refusal; put never fails

        SGCL_INLINE_HOT bool put(const uint8_t* p, size_t n) noexcept {
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

        SGCL_INLINE_HOT bool put(const uint8_t* p, size_t n) {
            auto w = out.write(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            if (!w) {
                failure = error(w.error(), written);
                return false;
            }
            written += n;
            return true;
        }
    };

    // Whether writing to a sink can throw: a vector's put cannot, a
    // stream's reaches the program's io::writer
    template<class Sink>
    inline constexpr bool NothrowSink = noexcept(std::declval<Sink&>().put(std::declval<const uint8_t*>(), size_t()));
}

