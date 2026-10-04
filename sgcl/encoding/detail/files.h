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
// the file opened for a reader, the text written with a new line after it.
// An error of the file rather than of its text (it does not open, read,
// write or close) has no place: "input/output error: open cfg.json: No
// such file or directory", not "offset 0: ..."
namespace sgcl::encoding::detail {
    // The stream's error as the error of a file: no place
    inline error file_error(const io::error& e) noexcept {
        error f(e, 0);
        return std::move(ErrorAccess::without_place(f));
    }

    // f(reader) over the file, the file closed after it; a file that does
    // not open is errc::io, one that fails while it is read the same, both
    // without a place
    template<class F>
    auto with_file(const string& path, F f) noexcept(noexcept(f(std::declval<const io::reader&>()))) -> decltype(f(std::declval<const io::reader&>())) {
        auto file = io::open(path);
        if (!file) {
            return unexpected(file_error(file.error()));
        }
        auto r = f(io::reader(*file));
        (void)file->close();
        if (!r && r.error().io_error()) {
            ErrorAccess::without_place(r.error());
        }
        return r;
    }

    // The text into the file, made or written over, a new line after it
    SGCL_INLINE_HOT expected<void, error> save_text(const string& path, const string& text) {
        std::string all(text.data(), text.size());
        all += '\n';
        if (auto w = io::write_file(path, string(all)); !w) {
            return unexpected(file_error(w.error()));
        }
        return {};
    }
}
