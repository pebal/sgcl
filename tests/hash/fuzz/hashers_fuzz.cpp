//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Every hasher of sgcl/hash fed any bytes in pieces of any sizes, against
// its own one-shot form: the streamed value must equal of(data), whatever
// the pieces (empty ones included) and wherever value() was asked on the
// way; a copy taken half way must go on as the original does; reset() must
// give a fresh hasher; digest() must be value() in big-endian bytes. The
// CRCs' resume() and combine() must give the CRC of the whole. The input:
// four bytes of piece sizes (each 0..765), eight of a seed (xxh3, xxh32, xxh64, maphash),
// sixteen of a SipHash key, then the message. No oracle outside the
// library: the vectors of tests/hash hold the one-shot forms to the
// references, this holds the streaming to the one-shot.
//
// Built with libFuzzer (tests/fuzz/run.sh tests/hash/fuzz/hashers_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/hash/hash.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    struct Pieces {
        size_t size[4];
        size_t at = 0;

        size_t next() {
            return size[at++ % 4];
        }
    };

    template<class V>
    bool equal(const V& a, const V& b) {
        if constexpr (std::is_integral_v<V>) {
            return a == b;
        } else {
            return std::memcmp(a.data(), b.data(), a.size()) == 0;
        }
    }

    // digest() is value() as big-endian bytes, for an integer value
    template<class H>
    void check_digest(const H& h) {
        auto v = h.value();
        auto d = h.digest();
        if constexpr (std::is_integral_v<decltype(v)>) {
            check(d.size() == sizeof v);
            for (size_t i = 0; i < d.size(); ++i) {
                check(uint8_t(d[i]) == uint8_t(v >> (8 * (d.size() - 1 - i))));
            }
        } else {
            check(std::memcmp(d.data(), v.data(), d.size()) == 0);
        }
    }

    // The data in pieces into a copy of `fresh`, value() asked after every
    // third piece, a copy taken at the middle and fed the rest too; both
    // must end at `whole`, and so must fresh after reset() and one update
    template<class H, class V>
    void streamed(const H& fresh, const uint8_t* p, size_t n, Pieces pz, const V& whole) {
        H h = fresh;
        optional<H> half;
        size_t piece = 0;
        for (size_t i = 0; i <= n;) {
            size_t take = std::min(pz.next(), n - i);
            h.update(view(p + i, take));
            if (half) {
                half->update(view(p + i, take));
            }
            i += take;
            if (++piece % 3 == 0) {
                (void)h.value();
            }
            if (!half && i >= n / 2) {
                half.emplace(h);
            }
            if (i == n) {
                break;
            }
        }
        check(equal(h.value(), whole));
        check(half && equal(half->value(), whole));
        check_digest(h);
        h.reset();
        h.update(view(p, n));
        check(equal(h.value(), whole));
    }

    template<class H>
    void plain(const uint8_t* p, size_t n, const Pieces& pz) {
        auto whole = H::of(view(p, n));
        streamed(H(), p, n, pz, whole);
    }

    template<class C>
    void crc_parts(const uint8_t* p, size_t n, size_t cut) {
        cut = n ? cut % (n + 1) : 0;
        auto whole = C::of(view(p, n));
        auto first = C::of(view(p, cut));
        auto second = C::of(view(p + cut, n - cut));
        check(C::combine(first, second, n - cut) == whole);
        auto r = C::resume(first);
        r.update(view(p + cut, n - cut));
        check(r.value() == whole);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 28) {
        return 0;
    }
    Pieces pz;
    for (int i = 0; i < 4; ++i) {
        pz.size[i] = size_t(data[i]) * 3;
    }
    if (pz.size[0] + pz.size[1] + pz.size[2] + pz.size[3] == 0) {
        pz.size[0] = 1;   // empty pieces, but not only empty ones
    }
    uint64_t seed = 0;
    std::memcpy(&seed, data + 4, 8);
    array<byte, 16> key;
    std::memcpy(key.data(), data + 12, 16);
    const uint8_t* p = data + 28;
    const size_t n = size - 28;

    plain<hash::adler32>(p, n, pz);
    plain<hash::crc32>(p, n, pz);
    plain<hash::crc32c>(p, n, pz);
    plain<hash::crc64>(p, n, pz);
    plain<hash::crc64_iso>(p, n, pz);
    plain<hash::fnv32>(p, n, pz);
    plain<hash::fnv32a>(p, n, pz);
    plain<hash::fnv64>(p, n, pz);
    plain<hash::fnv64a>(p, n, pz);
    plain<hash::fnv128>(p, n, pz);
    plain<hash::fnv128a>(p, n, pz);
    plain<hash::xxh3_64>(p, n, pz);
    plain<hash::xxh3_128>(p, n, pz);
    plain<hash::xxh32>(p, n, pz);
    plain<hash::xxh64>(p, n, pz);
    plain<hash::maphash>(p, n, pz);   // the process's seed, the same for of() and a new hasher

    streamed(hash::xxh3_64(seed), p, n, pz, hash::xxh3_64::of(view(p, n), seed));
    streamed(hash::xxh3_128(seed), p, n, pz, hash::xxh3_128::of(view(p, n), seed));
    streamed(hash::xxh32(uint32_t(seed)), p, n, pz, hash::xxh32::of(view(p, n), uint32_t(seed)));
    streamed(hash::xxh64(seed), p, n, pz, hash::xxh64::of(view(p, n), seed));
    streamed(hash::maphash(seed), p, n, pz, hash::maphash::of(view(p, n), seed));
    streamed(hash::siphash(key), p, n, pz, hash::siphash::of(view(p, n), key));

    crc_parts<hash::crc32>(p, n, size_t(seed));
    crc_parts<hash::crc32c>(p, n, size_t(seed >> 32));
    return 0;
}
