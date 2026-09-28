//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A 7z archive with a password from any bytes: opened from memory with the
// password "fuzz" and without one, every entry read whole and through the
// walk, the two agreeing where both succeed. The key derivation is held to
// 2^10 rounds here (SGCL_COMPRESS_7ZAES_FUZZ), so that an input costs
// microseconds and a k past it is refused as it is past 2^24 otherwise.
// The seeds have plain headers (and one a packed header, encrypted), so
// that the mutations reach the 7zAES properties — k, the salt, the IV,
// their sizes — and the encrypted data; each input is read as it is and
// again with the signature header's and the header's CRC-32s made right.
//
//   build-lzma/fuzz.sh sevenzip_aes 300
#define SGCL_COMPRESS_7ZAES_FUZZ 1
#include "sgcl/compress/sevenzip.h"

#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    void read_everything(const uint8_t* data, size_t size, bool password) {
        compress::limits l{uint64_t(1) << 24, uint64_t(64) << 20, 10000};
        compress::sevenzip::options o;
        o.limit = l;
        if (password) {
            o.password = string("fuzz");
        }
        auto a = compress::sevenzip::archive::from(slice<const std::byte>(reinterpret_cast<const std::byte*>(data), size), o);
        if (!a) {
            auto c = a.error().code();
            if (!password && c == compress::errc::wrong_password) {
                __builtin_trap();   // no password is never a wrong one
            }
            return;
        }
        std::vector<std::string> whole;
        for (auto& e : a->entries()) {
            auto d = a->read(e, l);
            if (!d && !password && d.error().code() == compress::errc::wrong_password) {
                __builtin_trap();
            }
            whole.push_back(d ? std::string(reinterpret_cast<const char*>(d->data()), d->size()) : std::string("\x01?failed"));
        }
        size_t i = 0;
        for (auto [e, r] : a->walk(l)) {
            if (e.size <= l.max_size) {
                auto all = r.read_all();
                if (all && whole[i] != "\x01?failed" && whole[i] != std::string(reinterpret_cast<const char*>(all->data()), all->size())) {
                    __builtin_trap();
                }
            }
            ++i;
        }
        if (i != whole.size()) {
            __builtin_trap();
        }
    }

    uint32_t crc(const uint8_t* p, size_t n) {
        return hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(p), n));
    }

    void put32(uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
    }

    uint64_t get64(const uint8_t* p) {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (8 * i);
        return v;
    }

    void both(const uint8_t* data, size_t size) {
        read_everything(data, size, true);
        read_everything(data, size, false);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    both(data, size);
    if (size >= 32) {
        std::vector<uint8_t> fixed(data, data + size);
        uint64_t offset = get64(fixed.data() + 12);
        uint64_t n = get64(fixed.data() + 20);
        if (offset <= size && n <= size - 32 && 32 + offset + n <= size) {
            put32(fixed.data() + 28, crc(fixed.data() + 32 + offset, size_t(n)));
        }
        put32(fixed.data() + 8, crc(fixed.data() + 12, 20));
        both(fixed.data(), fixed.size());
    }
    return 0;
}
