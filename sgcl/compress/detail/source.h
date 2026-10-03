//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "copy.h"
#include "stream.h"
#include "../../io/file.h"
#include "../../core/detail/bytes.h"

#include <cstdint>
#include <cstring>

namespace sgcl::compress::detail {
    // Where an archive's bytes come from (zip, 7z): a file read at offsets,
    // or bytes in memory (held when they are managed; an unmanaged buffer
    // is the caller's to keep)
    struct Source {
        io::file file;
        slice<const byte> memory;
        uint64_t size = 0;
        bool owned = false;   // opened by the archive from a path: closed by its close()

        expected<size_t, io::error> read_at(uint8_t* dst, size_t n, uint64_t off) const noexcept {
            if (!file) {
                if (off >= memory.size()) {
                    return 0;
                }
                size_t k = size_t(std::min<uint64_t>(n, memory.size() - off));
                copy_out(dst, bytes(memory) + off, k);
                return k;
            }
            size_t got = 0;
            while (got < n) {
                auto r = file.read_at(slice<byte>(reinterpret_cast<byte*>(dst + got), n - got), off + got);
                if (!r) {
                    return io::detail::fail(r);
                }
                if (*r == 0) {
                    break;
                }
                got += *r;
            }
            return got;
        }

        // A task's: the file's read runs on the blocking pool, so dst is
        // managed memory, and the slices given to the file carry its owner
        async::task<expected<size_t, io::error>> async_read_at(slice<byte> dst, uint64_t off) const noexcept {
            if (!file) {
                co_return read_at(reinterpret_cast<uint8_t*>(dst.data()), dst.size(), off);
            }
            size_t got = 0;
            while (got < dst.size()) {
                auto r = co_await file.async_read_at(dst.subslice(got), off + got);
                if (!r) {
                    co_return io::detail::fail(r);
                }
                if (*r == 0) {
                    break;
                }
                got += *r;
            }
            co_return got;
        }
    };
}
