//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"

namespace sgcl::concurrent {
    // Why bytes are not a sketch (bloom_filter, hyperloglog,
    // count_min_sketch from_bytes): a sentence and the byte of the input
    // the reading stopped on. One type of error for the whole module, the
    // shape of time::error.
    class error {
    public:
        SGCL_INLINE_HOT explicit error(const string& message, size_t offset = 0) noexcept
        : _message(message)
        , _offset(offset) {
        }

        SGCL_INLINE_HOT string message() const noexcept {
            return _message;
        }

        SGCL_INLINE_HOT size_t offset() const noexcept {
            return _offset;
        }

        // The same sentence at the same byte
        SGCL_INLINE_HOT friend bool operator==(const error& a, const error& b) noexcept {
            return a._offset == b._offset && a._message == b._message;
        }

    private:
        string _message;
        size_t _offset;
    };
}
