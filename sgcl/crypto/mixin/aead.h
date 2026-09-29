//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/vector.h"
#include "../constant_time.h"
#include "../detail/words.h"
#include "../error.h"
#include "../secret.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

// The interface every AEAD of the module shares (aes_gcm, chacha20_poly1305,
// xchacha20_poly1305), as Go's cipher.AEAD: seal and open, each into a new
// vector or into the caller's buffer, with or without additional data. The
// class supplies the two operations on raw bytes and the sizes; the mixin
// checks the contract (the nonce's length, the room in the output, the
// overlap, the largest message) and gives the forms.
//
// The contract, a broken one being an exception:
// - the nonce is exactly nonce_size bytes (std::invalid_argument);
// - the output of seal_to holds the plaintext and the tag, the output of
//   open_to the sealed data less the tag (std::length_error);
// - the output is the input itself or does not overlap it at all: the
//   work is done in place when out begins where the input begins
//   (std::invalid_argument for any other overlap);
// - a plaintext is at most max_plaintext_size bytes (std::length_error).
//
// A failed open is an error, errc::authentication, and nothing of the
// plaintext is ever written: the tag is computed and compared (in constant
// time) before the first byte is decrypted, and the output's bytes that an
// open would have written are zeroed, so that code which ignores the error
// reads zeros rather than the ciphertext or a part of a forgery's text.
namespace sgcl::crypto::mixin {
    template<class Derived>
    class aead {
    public:
        // Go's Seal with a nil dst: the ciphertext followed by the tag,
        // plaintext.size() + tag_size bytes
        vector<byte> seal(const slice<const byte>& nonce, const slice<const byte>& plaintext) const {
            return seal(nonce, plaintext, slice<const byte>());
        }

        vector<byte> seal(const slice<const byte>& nonce, const slice<const byte>& plaintext, const slice<const byte>& aad) const {
            _check_seal(nonce, plaintext.size());
            vector<byte> out(plaintext.size() + Derived::tag_size);
            _self()._seal(detail::bytes(nonce.data()), detail::bytes(plaintext.data()), plaintext.size(), detail::bytes(aad.data()), aad.size(), detail::bytes(out.data()));
            return out;
        }

        // The plaintext, or errc::authentication when the tag does not
        // match (the data, the nonce, the additional data or the key is
        // not the one sealed). The plaintext is the user's data, not key
        // material: a vector<byte>, as seal gives the ciphertext; open_to
        // writes into the caller's own buffer (for a key unwrapped, which
        // the caller then clears).
        [[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& nonce, const slice<const byte>& sealed) const {
            return open(nonce, sealed, slice<const byte>());
        }

        [[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& nonce, const slice<const byte>& sealed, const slice<const byte>& aad) const {
            _check_open(nonce);
            if (sealed.size() < Derived::tag_size || uint64_t(sealed.size() - Derived::tag_size) > Derived::max_plaintext_size) {
                return unexpected(error(errc::authentication));
            }
            const size_t n = sealed.size() - Derived::tag_size;
            vector<byte> out(n);
            if (!_self()._open(detail::bytes(nonce.data()), detail::bytes(sealed.data()), n, detail::bytes(sealed.data()) + n, detail::bytes(aad.data()), aad.size(), detail::bytes(out.data()))) {
                return unexpected(error(errc::authentication));
            }
            return out;
        }

        // Into the caller's buffer, nothing allocated: out holds at least
        // plaintext.size() + tag_size bytes and may be the plaintext itself
        // (sealed in place). The bytes written.
        size_t seal_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& plaintext) const {
            return seal_to(out, nonce, plaintext, slice<const byte>());
        }

        size_t seal_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& plaintext, const slice<const byte>& aad) const {
            _check_seal(nonce, plaintext.size());
            const size_t n = plaintext.size() + Derived::tag_size;
            if (out.size() < n) {
                throw length_error(std::string(Derived::_name) + "::seal_to: the output holds fewer bytes than the plaintext and the tag");
            }
            if (detail::inexact_overlap(out.data(), n, plaintext.data(), plaintext.size())) {
                throw invalid_argument(std::string(Derived::_name) + "::seal_to: the output overlaps the plaintext other than exactly");
            }
            _self()._seal(detail::bytes(nonce.data()), detail::bytes(plaintext.data()), plaintext.size(), detail::bytes(aad.data()), aad.size(), detail::bytes(out.data()));
            return n;
        }

        // Into the caller's buffer: out holds at least sealed.size() -
        // tag_size bytes and may be the sealed data itself (opened in
        // place). The plaintext's length, or errc::authentication with
        // those bytes of out zeroed.
        [[nodiscard]] expected<size_t, error> open_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& sealed) const {
            return open_to(out, nonce, sealed, slice<const byte>());
        }

        [[nodiscard]] expected<size_t, error> open_to(const slice<byte>& out, const slice<const byte>& nonce, const slice<const byte>& sealed, const slice<const byte>& aad) const {
            _check_open(nonce);
            if (sealed.size() < Derived::tag_size || uint64_t(sealed.size() - Derived::tag_size) > Derived::max_plaintext_size) {
                return unexpected(error(errc::authentication));
            }
            const size_t n = sealed.size() - Derived::tag_size;
            if (out.size() < n) {
                throw length_error(std::string(Derived::_name) + "::open_to: the output holds fewer bytes than the sealed data less the tag");
            }
            if (detail::inexact_overlap(out.data(), n, sealed.data(), sealed.size())) {
                throw invalid_argument(std::string(Derived::_name) + "::open_to: the output overlaps the sealed data other than exactly");
            }
            if (!_self()._open(detail::bytes(nonce.data()), detail::bytes(sealed.data()), n, detail::bytes(sealed.data()) + n, detail::bytes(aad.data()), aad.size(), detail::bytes(out.data()))) {
                return unexpected(error(errc::authentication));
            }
            return n;
        }

    private:
        const Derived& _self() const noexcept {
            return static_cast<const Derived&>(*this);
        }

        void _check_nonce(const slice<const byte>& nonce) const {
            _self()._check();
            if (nonce.size() != Derived::nonce_size) {
                throw invalid_argument(std::string(Derived::_name) + ": a nonce of " + std::to_string(nonce.size()) + " bytes, not " + std::to_string(Derived::nonce_size));
            }
        }

        void _check_seal(const slice<const byte>& nonce, size_t size) const {
            _check_nonce(nonce);
            if (uint64_t(size) > Derived::max_plaintext_size) {
                throw length_error(std::string(Derived::_name) + ": a plaintext longer than the cipher can seal under one nonce");
            }
        }

        void _check_open(const slice<const byte>& nonce) const {
            _check_nonce(nonce);
        }
    };
}
