//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// bzip2 on any bytes, two ways:
//   - the bytes as a stream: the decoder in memory and through the reader
//     fed in pieces of every size the last byte picks; and against libbz2:
//     a stream libbz2 takes whole, the library takes, with the same bytes
//     (the other way is not asked: libbz2 takes randomised blocks, which the
//     library refuses as Go does, and it finds some errors at other bits);
//   - the bytes as data: compressed at the level the first byte picks, by
//     compress and by the writer in pieces (flushed or not), decoded by the
//     library and by libbz2 to the same.
#include "sgcl/compress/compress.h"

#include <bzlib.h>

#include <cstring>
#include <string>

namespace {
    using namespace sgcl;

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

    // libbz2's decoding of the first stream; ok when it ends the stream
    // at the end of the input
    std::string bz_decompress(const uint8_t* data, size_t size, bool& ok) {
        bz_stream s{};
        BZ2_bzDecompressInit(&s, 0, 0);
        std::string out;
        char buf[1 << 14];
        s.next_in = reinterpret_cast<char*>(const_cast<uint8_t*>(data));
        s.avail_in = unsigned(size);
        int r;
        do {
            s.next_out = buf;
            s.avail_out = sizeof(buf);
            r = BZ2_bzDecompress(&s);
            out.append(buf, sizeof(buf) - s.avail_out);
        } while (r == BZ_OK && out.size() < (1u << 24) && (s.avail_in || s.avail_out == 0));
        ok = r == BZ_STREAM_END && s.avail_in == 0;
        BZ2_bzDecompressEnd(&s);
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    size_t step = 1 + data[size - 1] % 13;
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    auto whole = compress::bzip2::decompress(in, compress::limits{uint64_t(1) << 24});
    {
        compress::bzip2::reader r(pieces{data, size, step});
        std::byte buf[4096];
        std::string got;
        bool failed = false;
        for (size_t guard = 0; guard < 8192; ++guard) {
            auto n = r.read(buf);
            if (!n) {
                failed = true;
                break;
            }
            if (*n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
        }
        // the reader and the whole-memory form agree on data that decodes
        if (whole && got.size() < (1u << 24)) {
            if (failed || got != std::string(reinterpret_cast<const char*>(whole->data()), whole->size())) {
                __builtin_trap();
            }
        }
    }
    // libbz2 takes it: so does the library, with the same bytes
    bool ok = false;
    auto z = bz_decompress(data, size, ok);
    if (ok && z.size() < (1u << 24)) {
        if (!whole) {
            if (whole.error().code() == compress::errc::unsupported) {
                return 0;   // a randomised block
            }
            __builtin_trap();
        }
        if (std::string(reinterpret_cast<const char*>(whole->data()), whole->size()) != z) {
            __builtin_trap();
        }
    }
    // as data
    if (size >= 2) {
        const int level = 1 + data[0] % 9;
        const bool flush = data[0] & 0x80;
        const uint8_t* p = data + 1;
        const size_t n = size - 1;
        const std::string plain(reinterpret_cast<const char*>(p), n);
        auto c = compress::bzip2::compress(slice<const std::byte>(reinterpret_cast<const std::byte*>(p), n), {.level = level});
        auto back = compress::bzip2::decompress(c);
        if (!back || std::string(reinterpret_cast<const char*>(back->data()), back->size()) != plain) {
            __builtin_trap();
        }
        bool fine = false;
        if (bz_decompress(reinterpret_cast<const uint8_t*>(c.data()), c.size(), fine) != plain || !fine) {
            __builtin_trap();
        }
        io::buffer sink;
        {
            compress::bzip2::writer w(sink, {.level = level});
            for (size_t at = 0; at < n;) {
                const size_t k = std::min(n - at, step * 97);
                if (!w.write(slice<const std::byte>(reinterpret_cast<const std::byte*>(p + at), k))) {
                    __builtin_trap();
                }
                at += k;
                if (flush && !w.flush()) {
                    __builtin_trap();
                }
            }
            if (!w.close()) {
                __builtin_trap();
            }
        }
        auto streamed = compress::bzip2::decompress(sink.data());
        if (!streamed || std::string(reinterpret_cast<const char*>(streamed->data()), streamed->size()) != plain) {
            __builtin_trap();
        }
    }
    return 0;
}
