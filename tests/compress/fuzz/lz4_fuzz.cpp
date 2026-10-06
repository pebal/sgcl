//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// LZ4 on any bytes, three ways:
//   - the bytes as frames: decompress in memory and the reader fed in
//     pieces of every size the last byte picks must agree; and against
//     liblz4's LZ4F_decompress: what liblz4 decodes to its end, the
//     library decodes too, to the same bytes (the library also reads
//     legacy frames, which LZ4F does not, so not the other way round);
//   - the bytes as a raw block: what LZ4_decompress_safe decodes, the
//     library's decompress_block decodes to the same bytes;
//     but for a match of offset 0, which the block format calls invalid and
//     liblz4 1.10 decodes (from the output's own position), and the library
//     refuses (errc::corrupt, "a match of offset 0"), frames and blocks alike;
//   - the bytes as data: compressed at a level and with frame options the
//     first bytes pick, by compress, by the writer in pieces and by
//     compress_block, decoded by the library and by liblz4 to the same.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/lz4/include -L/opt/homebrew/opt/lz4/lib -llz4" \
//       tests/fuzz/run.sh tests/compress/fuzz/lz4_fuzz.cpp 300
#include "sgcl/compress/compress.h"

#include <lz4.h>
#include <lz4frame.h>

#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    using lz4 = compress::lz4;

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

    std::string lz4f(const uint8_t* data, size_t size, bool& ok) {
        LZ4F_dctx* d = nullptr;
        LZ4F_createDecompressionContext(&d, LZ4F_VERSION);
        std::string out;
        std::string buf(1 << 16, 0);
        size_t at = 0;
        ok = false;
        size_t r = 1;
        while (at < size && out.size() < Cap) {
            size_t dst = buf.size(), src = size - at;
            r = LZ4F_decompress(d, buf.data(), &dst, data + at, &src, nullptr);
            if (LZ4F_isError(r)) {
                LZ4F_freeDecompressionContext(d);
                return out;
            }
            out.append(buf.data(), dst);
            at += src;
            if (r == 0 && at < size) {
                LZ4F_resetDecompressionContext(d);   // another frame follows
            }
            if (src == 0 && dst == 0) {
                break;
            }
        }
        ok = r == 0 && at == size;
        LZ4F_freeDecompressionContext(d);
        return out;
    }

    std::string str(const vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    bool offset_zero(const compress::error& e) {
        return std::string(e.message().view()).find("a match of offset 0") != std::string::npos;
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
    compress::limits l {Cap, uint64_t(64) << 20};

    // as frames
    auto whole = lz4::decompress(view(data, size), l);
    {
        lz4::reader r(pieces {data, size, step}, l);
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
    // (a frame that names a dictionary liblz4 reads without one as long
    // as no match reaches before the data; the library asks for it)
    bool ok = false;
    std::string theirs = lz4f(data, size, ok);
    if (ok && theirs.size() <= Cap && !(!whole && (whole.error().code() == compress::errc::dictionary_required || offset_zero(whole.error())))) {
        check(whole && str(*whole) == theirs);
    }

    // as a raw block
    {
        const size_t room = size * 8 + 64;
        std::string out(room, 0);
        int n = LZ4_decompress_safe(reinterpret_cast<const char*>(data), out.data(), int(size), int(room));
        auto ours = lz4::decompress_block(view(data, size), room);
        if (n >= 0 && !(!ours && offset_zero(ours.error()))) {
            check(ours && str(*ours) == out.substr(0, size_t(n)));
        }
    }

    // as data
    if (size >= 2) {
        static const int levels[] = {1, 2, 3, 6, 9, 10, 12, -1, -20};
        const int level = levels[data[0] % 9];
        const uint8_t flags = data[1];
        lz4::options o {.level = level,
                        .block_size = lz4::block_size(4 + (flags & 3)),
                        .linked_blocks = bool(flags & 4),
                        .block_checksum = bool(flags & 8),
                        .content_checksum = bool(flags & 16)};
        const uint8_t* p = data + 2;
        const size_t n = size - 2;
        const std::string plain(reinterpret_cast<const char*>(p), n);
        auto c = lz4::compress(view(p, n), o);
        auto back = lz4::decompress(c, l);
        check(back && str(*back) == plain);
        bool lok = false;
        check(lz4f(reinterpret_cast<const uint8_t*>(c.data()), c.size(), lok) == plain && lok);
        io::buffer sink;
        {
            lz4::writer w(sink, o);
            size_t at = 0;
            while (at < n) {
                size_t k = std::min(n - at, step * 97);
                check(bool(w.write(view(p + at, k))));
                at += k;
                if (flags & 32) {
                    check(bool(w.flush()));
                }
            }
            check(bool(w.close()));
        }
        auto streamed = lz4::decompress(sink.data(), l);
        check(streamed && str(*streamed) == plain);
        auto raw = lz4::compress_block(view(p, n), o);
        std::string out(n + 1, 0);
        int k = LZ4_decompress_safe(reinterpret_cast<const char*>(raw.data()), out.data(), int(raw.size()), int(n));
        check(k == int(n) && out.substr(0, n) == plain);
    }
    return 0;
}
