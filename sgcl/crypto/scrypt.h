//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/scrypt.h"
#include "pbkdf2.h"
#include "secret.h"
#include "secure_zero.h"
#include "sha256.h"
#include "../core/aliases.h"
#include "../core/slice.h"

#include <cstddef>
#include <cstdint>

// scrypt (RFC 7914), Go's golang.org/x/crypto/scrypt: a key from a
// password, memory-hard — 128 r N bytes written and read back at indices
// the data chooses — so that each guess of an attacker costs memory as
// well as time. PBKDF2-HMAC-SHA256 of one iteration spreads the password
// over p blocks, each block goes through ROMix, and a second PBKDF2 over
// them gives the key. The format of Litecoin, of Tarsnap, of many wallets
// and of OpenSSH's sk keys; for storing passwords, argon2 is the better
// choice and the one with a stored form.
namespace sgcl::crypto {
    class scrypt {
    public:
        // The costs of RFC 7914 §2. The defaults are Go's documented choice
        // for an interactive login (N = 32768, r = 8, p = 1: 32 MiB)
        struct options {
            uint32_t cost = 32768;        // N: a power of two above 1, the blocks of memory
            uint32_t block_size = 8;      // r: a block is 128 r bytes
            uint32_t parallelism = 1;     // p: the blocks ROMix is run over, one after the other
        };

        // n bytes derived from password and salt with the costs of o; costs
        // out of their ranges are std::invalid_argument
        static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n = 32) {
            return derive(password, salt, n, options{});
        }

        static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n, const options& o) {
            _check(n, o);
            secret_bytes out(n);
            _derive(out.as_slice(), password, salt, o);
            return out;
        }

        // derive() into the caller's buffer: out.size() bytes. The salt is
        // read again at the end, so an out over it is std::invalid_argument
        // (the password, read whole first, may lie under out)
        static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt) {
            derive_to(out, password, salt, options{});
        }

        static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt,
                              const options& o) {
            _check(out.size(), o);
            if (detail::overlap(out.data(), out.size(), salt.data(), salt.size())) {
                throw invalid_argument("sgcl::crypto::scrypt: the output overlaps the salt");
            }
            _derive(out, password, salt, o);
        }

    private:
        static void _check(size_t n, const options& o) {
            const uint64_t N = o.cost, r = o.block_size, p = o.parallelism;
            if (N < 2 || (N & (N - 1)) != 0) {
                throw invalid_argument("sgcl::crypto::scrypt: a cost that is not a power of two above 1");
            }
            if (r == 0 || p == 0 || r * p >= (uint64_t(1) << 30)) {
                throw invalid_argument("sgcl::crypto::scrypt: r and p of 0, or r p past 2^30");
            }
            // 128 r N bytes of memory and 128 r p of blocks, within the address space
            if (N > (uint64_t(SIZE_MAX) / 128) / r || p > (uint64_t(SIZE_MAX) / 128) / r) {
                throw invalid_argument("sgcl::crypto::scrypt: more memory than the address space");
            }
            if (uint64_t(n) > 0xffffffffull * 32) {
                throw invalid_argument("sgcl::crypto::scrypt: more than 2^32 - 1 blocks of output");
            }
        }

        static void _derive(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt,
                            const options& o) {
            const size_t r = o.block_size, p = o.parallelism, block = 128 * r;
            // the p blocks, then N blocks of scratch and two of work
            detail::ScryptMemory memory((p * block + size_t(o.cost) * block + 2 * block) / 4);
            byte* b = reinterpret_cast<byte*>(memory.words);
            uint32_t* v = memory.words + p * block / 4;
            uint32_t* xy = v + size_t(o.cost) * block / 4;
            hmac<sha256> mac(password);
            _pbkdf2(mac, slice<byte>(b, p * block), salt);
            for (size_t i = 0; i < p; ++i) {
                detail::scrypt_ro_mix(detail::bytes(b + i * block), uint32_t(r), o.cost, v, xy);
            }
            _pbkdf2(mac, out, slice<const byte>(b, p * block));
        }

        // PBKDF2-HMAC-SHA256 of one iteration: T_i = HMAC(P, S || INT(i))
        static void _pbkdf2(const hmac<sha256>& mac, const slice<byte>& out, const slice<const byte>& salt) noexcept {
            for (size_t done = 0, i = 1; done < out.size(); ++i) {
                hmac<sha256> m = mac.clone();
                m.update(salt);
                unsigned char index[4];
                detail::store_be32(index, uint32_t(i));
                m.update(slice<const byte>(reinterpret_cast<const byte*>(index), 4));
                auto t = m.value();
                size_t take = std::min<size_t>(32, out.size() - done);
                sgcl::detail::copy_bytes(out.data() + done, t.data(), take);
                detail::secure_zero(t.data(), t.size());
                done += take;
            }
        }
    };
}
