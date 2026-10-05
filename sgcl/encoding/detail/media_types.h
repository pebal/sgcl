//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <string_view>

// The media type of a file by its name: a mail's attachment (email.h), and
// through net::http::detail the file server's (serve.h) and a file part's
// of a form (form.h)
namespace sgcl::encoding::detail {
    // The Content-Type of a file by its extension, as Go's built-in
    // table and mime.types have it; application/octet-stream for any
    // other (Go sniffs the first bytes; this does not)
    inline const char* content_type_of(std::string_view name) noexcept {
        const auto dot = name.rfind('.');
        const auto slash = name.rfind('/');
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
            return "application/octet-stream";
        }
        char ext[8] = {};
        const std::string_view e = name.substr(dot + 1);
        if (e.size() >= sizeof(ext)) {
            return "application/octet-stream";
        }
        for (size_t i = 0; i < e.size(); ++i) {
            const char c = e[i];
            ext[i] = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
        }
        static constexpr struct {
            const char* ext;
            const char* type;
        } table[] = {
            {"html", "text/html; charset=utf-8"},
            {"htm", "text/html; charset=utf-8"},
            {"css", "text/css; charset=utf-8"},
            {"js", "text/javascript; charset=utf-8"},
            {"mjs", "text/javascript; charset=utf-8"},
            {"json", "application/json"},
            {"txt", "text/plain; charset=utf-8"},
            {"md", "text/markdown; charset=utf-8"},
            {"csv", "text/csv; charset=utf-8"},
            {"xml", "text/xml; charset=utf-8"},
            {"svg", "image/svg+xml"},
            {"png", "image/png"},
            {"jpg", "image/jpeg"},
            {"jpeg", "image/jpeg"},
            {"gif", "image/gif"},
            {"webp", "image/webp"},
            {"avif", "image/avif"},
            {"heic", "image/heic"},
            {"ico", "image/vnd.microsoft.icon"},
            {"pdf", "application/pdf"},
            {"wasm", "application/wasm"},
            {"woff", "font/woff"},
            {"woff2", "font/woff2"},
            {"ttf", "font/ttf"},
            {"otf", "font/otf"},
            {"mp3", "audio/mpeg"},
            {"wav", "audio/wav"},
            {"ogg", "audio/ogg"},
            {"mp4", "video/mp4"},
            {"webm", "video/webm"},
            {"zip", "application/zip"},
            {"gz", "application/gzip"},
            {"tar", "application/x-tar"},
            {"7z", "application/x-7z-compressed"},
        };
        const std::string_view x(ext, e.size());
        for (const auto& t : table) {
            if (x == t.ext) {
                return t.type;
            }
        }
        return "application/octet-stream";
    }
}
