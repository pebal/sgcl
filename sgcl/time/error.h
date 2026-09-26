//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"

namespace sgcl::time {
    // Why a text is not a date, or a file not a time zone:
    // a sentence and the byte of the input the reading stopped on. One
    // type of error for the whole module.
    class error {
    public:
        explicit error(const string& message, size_t offset = 0)
        : _message(message)
        , _offset(offset) {
        }

        string message() const {
            return _message;
        }

        size_t offset() const noexcept {
            return _offset;
        }

        // The same sentence at the same byte
        friend bool operator==(const error& a, const error& b) noexcept {
            return a._offset == b._offset && a._message == b._message;
        }

    private:
        string _message;
        size_t _offset;
    };
}
