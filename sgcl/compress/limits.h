//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace sgcl::compress {
    // A limit on what a decompression of data in memory makes: data from
    // outside may decompress a thousand times over (a "bomb"), so the
    // functions of the module stop at max_size bytes with errc::too_large.
    // limits{UINT64_MAX} lifts it. A stream has none: its reader decides
    // how much it reads (io::limit_reader over it). max_memory bounds the
    // memory a decoder takes because the data asks for it (LZMA's
    // dictionary, whose size the header gives), in memory and in a
    // stream alike: more is errc::too_large before anything is allocated.
    // max_entries bounds the entries an archive's header may list (7z):
    // its table is made before any entry is read.
    struct limits {
        uint64_t max_size = uint64_t(1) << 30;
        uint64_t max_memory = uint64_t(1) << 30;
        uint64_t max_entries = 1000000;
    };
}
