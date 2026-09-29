//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../async/blocking.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/string.h"
#include "../../io/file.h"

#include <string>

// What the formats' load and save share (json, xml, csv; DESIGN 285/312):
// the file opened for a reader, the text written with a new line after it
namespace sgcl::encoding::detail {
    // f(reader) over the file, the file closed after it; a file that does
    // not open is errc::io
    template<class F>
    auto with_file(const string& path, F f) -> decltype(f(std::declval<const io::reader&>())) {
        auto file = io::open(path);
        if (!file) {
            return unexpected(error(file.error(), 0));
        }
        auto r = f(io::reader(*file));
        (void)file->close();
        return r;
    }

    // The text into the file, made or written over, a new line after it
    inline expected<void, error> save_text(const string& path, const string& text) {
        std::string all(text.data(), text.size());
        all += '\n';
        if (auto w = io::write_file(path, string(all)); !w) {
            return unexpected(error(w.error(), 0));
        }
        return {};
    }
}
