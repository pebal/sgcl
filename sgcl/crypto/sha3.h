//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/keccak.h"
#include "secret.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../hash/mixin/hasher.h"

#include <cstddef>

// SHA-3 (FIPS 202), Go's crypto/sha3: the four digests SHA3-224, -256, -384
// and -512, and the two extendable-output functions SHAKE128 and SHAKE256.
// All six are the Keccak-f[1600] sponge, differing in the rate (the bytes
// of the state a block fills) and in the domain bits of the padding; a
// digest of SHA-3 is not a digest of Keccak (Ethereum's Keccak-256 pads
// otherwise and has other values).
//
// A digest is a hasher in the shape of the hash module's
// (hash::mixin::hasher). A SHAKE is not: its output has no fixed length, it
// is read — read(n) as many bytes as asked, and read again for the bytes
// after them — and once read it takes no more input.
//
// Each holds the state of 200 bytes and a position, a plain value that a
// copy branches. On arm64 the permutation runs on the SHA-3 instructions of
// ARMv8.2 (detail/keccak.h).
namespace sgcl::crypto {
    namespace detail {
        // The four digests of SHA-3: one sponge, the rate 200 - 2 * size
        template<class Derived, size_t Size>
        class Sha3 : public hash::mixin::hasher<Derived> {
            friend struct HashAccess;

        public:
            using hash::mixin::hasher<Derived>::update;

            static constexpr size_t digest_size = Size;
            static constexpr size_t block_size = 200 - 2 * Size;

            Sha3() noexcept {
                _state.init();
            }

            void update(const slice<const byte>& data) noexcept {
                _state.absorb(bytes(data.data()), data.size());
            }

            // The digest of everything so far; the hasher goes on
            array<byte, Size> value() const noexcept {
                Sha3 h = *this;
                array<byte, Size> out;
                h._finish(bytes(out.data()));
                return out;
            }

            array<byte, Size> digest() const noexcept {
                return value();
            }

            void reset() noexcept {
                _state.init();
            }

        private:
            KeccakSponge<block_size> _state;

            // the domain bits 01 and the padding: the first byte 0x06
            void _finish(unsigned char* out) noexcept {
                _state.pad(0x06);
                _state.squeeze(out, Size);
            }
        };

        // SHAKE128 and SHAKE256: the sponge of rate 168 or 136, absorbing
        // until the first read, squeezing after it
        template<size_t Rate>
        class Shake {
        public:
            static constexpr size_t block_size = Rate;

            Shake() noexcept {
                _state.init();
            }

            // Bytes or text, the forms a hasher's update() takes. After the
            // first read the input is closed: std::invalid_argument
            void update(const slice<const byte>& data) {
                if (_reading) {
                    throw invalid_argument("sgcl::crypto::shake: update after read");
                }
                _state.absorb(bytes(data.data()), data.size());
            }

            // The next n bytes of the output, as many as asked: the first
            // read ends the input, and each read goes on where the last one
            // stopped (two reads of 16 give the bytes of one read of 32).
            // Taken as a secret (SHAKE derives keys as often as not): up to
            // 64 bytes in the secret_bytes itself; read_to for a buffer of
            // one's own
            secret_bytes read(size_t n) noexcept {
                secret_bytes out(n);
                read_to(out.as_slice());
                return out;
            }

            // The next out.size() bytes of the output into out, no
            // allocation
            void read_to(const slice<byte>& out) noexcept {
                if (!_reading) {
                    _state.pad(0x1f);   // the domain bits 1111 and the padding
                    _reading = true;
                }
                _state.squeeze(bytes(out.data()), out.size());
            }

            // As new: the input open again
            void reset() noexcept {
                _state.init();
                _reading = false;
            }

        protected:
            KeccakSponge<Rate> _state;
            bool _reading = false;
        };
    }

    class sha3_224 : public detail::Sha3<sha3_224, 28> {
    };

    class sha3_256 : public detail::Sha3<sha3_256, 32> {
    };

    class sha3_384 : public detail::Sha3<sha3_384, 48> {
    };

    class sha3_512 : public detail::Sha3<sha3_512, 64> {
    };

    // SHAKE128: 128 bits of security against every attack when at least 32
    // bytes are read (collisions need twice the output), Go's
    // sha3.NewShake128
    class shake128 : public detail::Shake<168> {
    public:
        // The first n bytes of the output over data, in one call
        static secret_bytes of(const slice<const byte>& data, size_t n) noexcept {
            shake128 x;
            x.update(data);
            return x.read(n);
        }
    };

    // SHAKE256: 256 bits of security when at least 64 bytes are read
    class shake256 : public detail::Shake<136> {
    public:
        static secret_bytes of(const slice<const byte>& data, size_t n) noexcept {
            shake256 x;
            x.update(data);
            return x.read(n);
        }
    };

    namespace detail {
        template<>
        inline constexpr bool crypto_digest<sha3_224> = true;

        template<>
        inline constexpr bool crypto_digest<sha3_256> = true;

        template<>
        inline constexpr bool crypto_digest<sha3_384> = true;

        template<>
        inline constexpr bool crypto_digest<sha3_512> = true;
    }
}
