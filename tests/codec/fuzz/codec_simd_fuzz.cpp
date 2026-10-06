//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The codec's vector kernels (simd.h: NEON, SSE2) against their plain twins
// on any bytes: the first picks the kernel, the rest are its input. The
// dispatching entry (the vector road with the plain one on what it leaves)
// and the plain road give the same bytes; every buffer is allocated to its
// exact length, so a vector load or store past an end is AddressSanitizer's.
//   0  IDCT islow: 64 coefficients and 64 quantizers, an 8x8 block out
//   1  YCbCr to RGB: three planes of a third of the rest each
//   2  PNG unfilter: the filter and the bytes per pixel from the next byte,
//      a raw row and the prior row of half the rest each
//   3  FDCT islow and quantization: 64 samples, a stride, 64 quantizers
//      (the encoder's: 1..255)
//   4  RGB to YCbCr: pixels of three bytes
//   5  subsampling across (4:2:2) and both ways (4:2:0): two rows of an
//      even length
//   6  the encoder's mask of nonzero coefficients: 64 coefficients
//   7  the encoder's block made ready for the coder: 64 coefficients
//   8  the decoder's upsampling (h2, v2 both ways, h2v2): a row and the
//      one near it, half the rest each
//   9  VP8L's predictors that read the pixel to the left: a mode, a row
//      above and a row of residuals
//  10  image's eight transforms (orient.h): the bytes of a pixel, the
//      transform and the sides from the next bytes, the pixels the rest
//  11  the VP8 encoder's residue DCT, quantizer and squared error: two
//      blocks of pixels, the quantizers and first from the next bytes
#include "sgcl/codec/detail/jpeg_color.h"
#include "sgcl/codec/detail/jpeg_fdct.h"
#include "sgcl/codec/detail/jpeg_idct.h"
#include "sgcl/codec/detail/orient.h"
#include "sgcl/codec/detail/png_filter.h"
#include "sgcl/codec/detail/vp8_encoder.h"
#include "sgcl/codec/detail/vp8l_simd.h"
#include "sgcl/core/detail/bytes.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {
    using namespace sgcl::codec::detail;
    using bytes = std::unique_ptr<uint8_t[]>;

    bytes copy(const uint8_t* data, size_t n) {
        bytes b(new uint8_t[n]);
        if (n) {
            sgcl::detail::copy_bytes(b.get(), data, n);
        }
        return b;
    }

    void idct_case(const uint8_t* data, size_t size) {
        int16_t coef[64] = {};
        uint16_t quant[64] = {};
        sgcl::detail::copy_bytes(coef, data, std::min(size, sizeof coef));
        if (size > sizeof coef) {
            sgcl::detail::copy_bytes(quant, data + sizeof coef, std::min(size - sizeof coef, sizeof quant));
        }
        // a zero quantizer is not in any file (DQT refuses it), but the
        // kernels need not care: both roads multiply by it
        bytes a(new uint8_t[64]), b(new uint8_t[64]);
        idct_islow_plain(coef, quant, a.get(), 8);
        idct_islow(coef, quant, b.get(), 8);
        if (std::memcmp(a.get(), b.get(), 64) != 0) {
            std::abort();
        }
    }

    void ycc_case(const uint8_t* data, size_t size) {
        const size_t n = size / 3;
        bytes y = copy(data, n), cb = copy(data + n, n), cr = copy(data + 2 * n, n);
        bytes a(new uint8_t[3 * n]), b(new uint8_t[3 * n]);
        ycc_to_rgb_plain(y.get(), cb.get(), cr.get(), a.get(), n);
        ycc_to_rgb(y.get(), cb.get(), cr.get(), b.get(), n);
        if (n && std::memcmp(a.get(), b.get(), 3 * n) != 0) {
            std::abort();
        }
    }

    void unfilter_case(const uint8_t* data, size_t size) {
        if (size == 0) {
            return;
        }
        static constexpr unsigned depths[] = {1, 2, 3, 4, 6, 8};
        const uint8_t type = uint8_t(data[0] % 5);
        const unsigned bpp = depths[(data[0] / 5) % 6];
        ++data;
        --size;
        const size_t n = size / 2 / bpp * bpp;
        bytes raw = copy(data, n), prior = copy(data + n, n);
        bytes a(new uint8_t[n]), b(new uint8_t[n]);
        unfilter_plain(type, raw.get(), prior.get(), a.get(), n, bpp);
        unfilter(type, raw.get(), prior.get(), b.get(), n, bpp);
        if (n && std::memcmp(a.get(), b.get(), n) != 0) {
            std::abort();
        }
    }

    void fdct_case(const uint8_t* data, size_t size) {
        if (size == 0) {
            return;
        }
        const size_t stride = 8 + data[0] % 8;
        ++data;
        --size;
        bytes in(new uint8_t[7 * stride + 8]);
        std::memset(in.get(), 0, 7 * stride + 8);
        sgcl::detail::copy_bytes(in.get(), data, std::min(size, 7 * stride + 8));
        int32_t a[64], b[64];
        fdct_islow_plain(in.get(), stride, a);
        fdct_islow(in.get(), stride, b);
        if (std::memcmp(a, b, sizeof a) != 0) {
            std::abort();
        }
        uint16_t table[64];
        for (int i = 0; i < 64; ++i) {
            const size_t k = 7 * stride + 8 + size_t(i);
            table[i] = uint16_t(1 + (k < size ? data[k] : i) % 255);
        }
        int16_t qa[64], qb[64];
        quantize_plain(a, table, qa);
        quantize(a, QuantSteps(table), qb);
        if (std::memcmp(qa, qb, sizeof qa) != 0) {
            std::abort();
        }
    }

    void rgb_case(const uint8_t* data, size_t size) {
        const size_t n = size / 3;
        bytes rgb = copy(data, 3 * n);
        bytes y0(new uint8_t[n]), cb0(new uint8_t[n]), cr0(new uint8_t[n]);
        bytes y1(new uint8_t[n]), cb1(new uint8_t[n]), cr1(new uint8_t[n]);
        rgb_to_ycc_plain(rgb.get(), y0.get(), cb0.get(), cr0.get(), n);
        rgb_to_ycc(rgb.get(), y1.get(), cb1.get(), cr1.get(), n);
        if (n && (std::memcmp(y0.get(), y1.get(), n) != 0 || std::memcmp(cb0.get(), cb1.get(), n) != 0 || std::memcmp(cr0.get(), cr1.get(), n) != 0)) {
            std::abort();
        }
    }

    void downsample_case(const uint8_t* data, size_t size) {
        const size_t n = size / 4;
        bytes a = copy(data, 2 * n), b = copy(data + 2 * n, 2 * n);
        bytes p(new uint8_t[n]), v(new uint8_t[n]);
        downsample::h2v1_plain(a.get(), p.get(), n);
        downsample::h2v1(a.get(), v.get(), n);
        if (n && std::memcmp(p.get(), v.get(), n) != 0) {
            std::abort();
        }
        downsample::h2v2_plain(a.get(), b.get(), p.get(), n);
        downsample::h2v2(a.get(), b.get(), v.get(), n);
        if (n && std::memcmp(p.get(), v.get(), n) != 0) {
            std::abort();
        }
    }

    void mask_case(const uint8_t* data, size_t size) {
        int16_t z[64] = {};
        sgcl::detail::copy_bytes(z, data, std::min(size, sizeof z));
        if (nonzero_mask(z) != nonzero_mask_plain(z)) {
            std::abort();
        }
    }

    void coded_case(const uint8_t* data, size_t size) {
        int16_t block[64] = {};
        sgcl::detail::copy_bytes(block, data, std::min(size, sizeof block));
        CodedBlock a, b;
        coded_block_plain(block, a);
        coded_block(block, b);
        if (std::memcmp(a.z, b.z, sizeof a.z) != 0 || std::memcmp(a.bits, b.bits, sizeof a.bits) != 0 ||
            std::memcmp(a.size, b.size, sizeof a.size) != 0 || a.mask != b.mask) {
            std::abort();
        }
    }

    void upsample_case(const uint8_t* data, size_t size) {
        const size_t n = size / 2;
        if (n < 3) {
            return;
        }
        bytes row = copy(data, n), near = copy(data + n, n);
        bytes a(new uint8_t[2 * n]), b(new uint8_t[2 * n]);
        auto same = [&](size_t len) {
            if (std::memcmp(a.get(), b.get(), len) != 0) {
                std::abort();
            }
        };
        upsample::h2_plain(row.get(), a.get(), n);
        upsample::h2(row.get(), b.get(), n);
        same(2 * n);
        upsample::h2v2_plain(row.get(), near.get(), a.get(), n);
        upsample::h2v2(row.get(), near.get(), b.get(), n);
        same(2 * n);
        for (bool lower : {false, true}) {
            upsample::v2_plain(row.get(), near.get(), lower, a.get(), n);
            upsample::v2(row.get(), near.get(), lower, b.get(), n);
            same(n);
        }
    }

    // VP8L's predictors that read L: a mode, then a row above and a row of
    // residuals from the rest, the run by mode against the plain road
    void predict_left_case(const uint8_t* data, size_t size) {
        if (size < 1) {
            return;
        }
        static constexpr unsigned modes[] = {1, 5, 6, 7, 10, 11, 12, 13};
        const unsigned mode = modes[data[0] % 8];
        ++data;
        --size;
        const size_t n = size / 8;
        if (n < 1) {
            return;
        }
        std::vector<uint32_t> top(n + 2), a(n + 1), b(n + 1);
        for (size_t i = 0; i < n + 2; ++i) {
            uint32_t v = 0;
            sgcl::detail::copy_bytes(&v, data + 4 * (i % (2 * n)), 4);
            top[i] = v;
        }
        for (size_t i = 0; i < n + 1; ++i) {
            uint32_t v = 0;
            sgcl::detail::copy_bytes(&v, data + 4 * ((n + i) % (2 * n)), 4);
            a[i] = v;
        }
        b = a;
        sgcl::codec::detail::vp8l::predict_left_run_plain(mode, a.data() + 1, top.data() + 1, n);
        sgcl::codec::detail::vp8l::predict_left_run(mode, b.data() + 1, top.data() + 1, n);
        if (a != b) {
            std::abort();
        }
    }
}

namespace {
    // image's transforms: the vector road against the plain one
    void orient_case(const uint8_t* data, size_t size) {
        if (size < 4) {
            return;
        }
        static constexpr unsigned sizes[] = {1, 2, 3, 4, 6, 8};
        const unsigned b = sizes[data[0] % 6];
        const unsigned o = 1 + data[1] % 8;
        const size_t w = 1 + data[2] % 80, h = 1 + data[3] % 80;
        data += 4;
        size -= 4;
        const size_t n = w * h * b;
        bytes src(new uint8_t[n]), x(new uint8_t[n]), y(new uint8_t[n]);
        for (size_t i = 0; i < n; ++i) {
            src[i] = size ? data[i % size] : uint8_t(i);
        }
        orient::apply(src.get(), w, h, b, o, x.get());
        orient::apply_plain(src.get(), w, h, b, o, y.get());
        if (std::memcmp(x.get(), y.get(), n) != 0) {
            std::abort();
        }
    }

    // the VP8 encoder's kernels: the vector road against the plain one
    void vp8_case(const uint8_t* data, size_t size) {
        if (size < 35) {
            return;
        }
        static constexpr int16_t qs[] = {4, 7, 8, 30, 64, 157, 200, 314};
        const vp8::Quantizer4 k(qs[data[0] % 8], qs[(data[0] >> 3) % 8]);
        const int first = data[1] & 1;
        uint8_t src[16], pred[16];
        sgcl::detail::copy_bytes(src, data + 2, 16);
        sgcl::detail::copy_bytes(pred, data + 18, 16);
        int16_t a[16], c[16];
        vp8::residue_dct(src, 4, pred, 4, a);
        vp8::residue_dct_plain(src, 4, pred, 4, c);
        if (std::memcmp(a, c, sizeof a) != 0) {
            std::abort();
        }
        int16_t la[16], lc[16], da[16], dc[16];
        if (vp8::quantize(a, first, k, la, da) != vp8::quantize_plain(a, first, k, lc, dc) || std::memcmp(la, lc, sizeof la) != 0 ||
            std::memcmp(da, dc, sizeof da) != 0) {
            std::abort();
        }
        if (vp8::sse(src, 4, pred, 4, 4, 4) != vp8::sse_plain(src, 4, pred, 4, 4, 4)) {
            std::abort();
        }
        const size_t n = std::min<size_t>(size - 34, 256);
        if (n == 256) {
            if (vp8::sse(data + 34, 16, data + 34, 16, 16, 16) != 0) {
                std::abort();
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    switch (data[0] % 12) {
    case 0:
        idct_case(data + 1, size - 1);
        break;
    case 1:
        ycc_case(data + 1, size - 1);
        break;
    case 2:
        unfilter_case(data + 1, size - 1);
        break;
    case 3:
        fdct_case(data + 1, size - 1);
        break;
    case 4:
        rgb_case(data + 1, size - 1);
        break;
    case 5:
        downsample_case(data + 1, size - 1);
        break;
    case 6:
        mask_case(data + 1, size - 1);
        break;
    case 7:
        coded_case(data + 1, size - 1);
        break;
    case 8:
        upsample_case(data + 1, size - 1);
        break;
    case 9:
        predict_left_case(data + 1, size - 1);
        break;
    case 10:
        orient_case(data + 1, size - 1);
        break;
    default:
        vp8_case(data + 1, size - 1);
        break;
    }
    return 0;
}
