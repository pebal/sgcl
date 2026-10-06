//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../time/datetime.h"

#include <cstdint>

namespace sgcl::net::webdav {
    // A resource as a PROPFIND tells of it: its path (the href decoded), a
    // collection or a file, and the live properties the server gave
    struct resource {
        string path;                         // "/docs/report.pdf", "/docs/" for a collection
        bool collection = false;             // a directory
        uint64_t size = 0;                   // getcontentlength; 0 for a collection
        optional<time::datetime> modified;   // getlastmodified
        string etag;                         // getetag, quotes kept
        string content_type;                 // getcontenttype

        friend bool operator==(const resource&, const resource&) noexcept = default;
    };
}
