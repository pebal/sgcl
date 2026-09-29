//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The private keys' PEM on any bytes: detail::read_key_pem (the blocks, the
// labels, the text passed over, the refusals) and from_pem of every type of
// key, the input taken as PEM text.
//
//   - a block read has a private key's label and no more DER than the text
//     could hold; written back by write_key_pem, it reads to the same label
//     and the same bytes;
//   - a key's from_pem takes the text only when read_key_pem does, and a
//     "PRIVATE KEY" block goes as its DER goes through from_pkcs8_der;
//   - a key taken writes itself as PEM (to_pem) that reads to the same key
//     and writes the same PEM again;
//   - the whole input as DER under a label, written and read, comes back
//     byte for byte.
//
// A disagreement aborts; ASan and UBSan catch the rest.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/key_pem_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"
#include "sgcl/crypto/detail/key_pem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {
    using namespace sgcl;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "key_pem_fuzz: %s\n", what);
            std::abort();
        }
    }

    bool known_label(std::string_view label) {
        for (auto k : crypto::detail::KeyLabels) {
            if (label == k) {
                return true;
            }
        }
        return false;
    }

    // One type of key over the text: from_pem against the reader, and a
    // key taken through its own PEM and back
    template<class K>
    void key(const slice<const byte>& text, const expected<crypto::detail::KeyPem, crypto::error>& block) {
        auto k = K::from_pem(text);
        if (block && block->label == "PRIVATE KEY") {
            check(bool(k) == bool(K::from_pkcs8_der(block->der)), "from_pem and from_pkcs8_der disagree on a PRIVATE KEY block");
        }
        if (!k) {
            return;
        }
        check(bool(block), "from_pem took a text the reader refuses");
        crypto::secret_bytes pem = k->to_pem();
        auto again = K::from_pem(pem);
        check(bool(again), "a key's own PEM does not read");
        check(again->to_pem() == pem, "a key's PEM read and written is another PEM");
        check(again->to_pkcs8_der() == k->to_pkcs8_der(), "a key's PEM reads to another key");
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const auto text = view(data, size);

    // the reader, and its block written back
    auto block = crypto::detail::read_key_pem(text);
    if (block) {
        check(known_label(block->label), "a block read under a label that is not a private key's");
        check(block->der.size() <= size, "more DER than the text holds");
        crypto::secret_bytes written = crypto::detail::write_key_pem(block->label, block->der);
        auto back = crypto::detail::read_key_pem(written);
        check(bool(back), "a block written does not read");
        check(back->label == block->label, "a block written reads under another label");
        check(back->der == block->der, "a block written reads to other bytes");
    }

    // every type of key
    key<crypto::ed25519::private_key>(text, block);
    key<crypto::x25519::private_key>(text, block);
    key<crypto::p256::private_key>(text, block);
    key<crypto::p256::ecdh_key>(text, block);
    key<crypto::p384::private_key>(text, block);
    key<crypto::p384::ecdh_key>(text, block);
    key<crypto::rsa::private_key>(text, block);

    // the input as DER: written under a label and read, byte for byte
    for (auto label : crypto::detail::KeyLabels) {
        crypto::secret_bytes written = crypto::detail::write_key_pem(label, text);
        auto back = crypto::detail::read_key_pem(written);
        check(back && back->label == label, "the input's DER written does not read under its label");
        check(back->der.as_slice().size() == size && (size == 0 || std::memcmp(back->der.as_slice().data(), data, size) == 0),
              "the input's DER written reads to other bytes");
    }
    return 0;
}
