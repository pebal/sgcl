//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Snappy on any bytes, three ways, with no oracle (no reference library is
// on this machine): the bytes as framed data, decompress in memory and the
// reader fed in pieces of every size the last byte picks must agree; the
// bytes as a block, decompress_block must make exactly the length its head
// says or fail, and decompressed_size must say that length; the bytes as
// data, compressed both ways (compress, compress_block, the writer in
// pieces, flushed where the first byte says) and decoded back to the same.
//
//   tests/fuzz/run.sh tests/compress/fuzz/snappy_fuzz.cpp 300
#include "sgcl/compress/compress.h"

#include <cstring>
#include <string>

namespace {
    using namespace sgcl;
    using snappy = compress::snappy;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct pieces {
        const uint8_t* data;
        size_t size;
        size_t step;
        size_t at = 0;

        expected<size_t, io::error> read(slice<std::byte> b) {
            size_t n = std::min({step, b.size(), size - at});
            std::memcpy(b.data(), data + at, n);
            at += n;
            return n;
        }
    };

    constexpr size_t Cap = size_t(1) << 22;

    std::string str(const vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    slice<const std::byte> view(const uint8_t* p, size_t n) {
        return slice<const std::byte>(reinterpret_cast<const std::byte*>(p), n);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const size_t step = 1 + data[size - 1] % 13;
    compress::limits l {Cap};

    // as framed data
    auto whole = snappy::decompress(view(data, size), l);
    {
        snappy::reader r(pieces {data, size, step});
        std::byte buf[4096];
        std::string got;
        bool failed = false;
        for (;;) {
            auto n = r.read(slice<std::byte>(buf, sizeof buf));
            if (!n) {
                failed = true;
                break;
            }
            if (*n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
            if (got.size() > Cap) {
                failed = true;
                break;
            }
        }
        if (whole) {
            check(!failed && got == str(*whole));
        } else if (whole.error().code() != compress::errc::too_large) {
            check(failed);
        }
    }

    // as a block
    {
        auto block = snappy::decompress_block(view(data, size), l);
        auto length = snappy::decompressed_size(view(data, size));
        if (block) {
            check(length && *length == block->size());
        }
    }

    // as data
    {
        const std::string plain(reinterpret_cast<const char*>(data), size);
        auto framed = snappy::compress(view(data, size));
        auto back = snappy::decompress(framed, l);
        check(back && str(*back) == plain);
        auto block = snappy::compress_block(view(data, size));
        check(block.size() <= 32 + size + size / 6);
        auto unblock = snappy::decompress_block(block, l);
        check(unblock && str(*unblock) == plain);
        io::buffer sink;
        {
            snappy::writer w(sink);
            size_t at = 0;
            while (at < size) {
                size_t k = std::min(size - at, step * 131);
                check(bool(w.write(view(data + at, k))));
                at += k;
                if (data[0] & 1) {
                    check(bool(w.flush()));
                }
            }
            check(bool(w.close()));
        }
        auto streamed = snappy::decompress(sink.data(), l);
        check(streamed && str(*streamed) == plain);
    }
    return 0;
}
