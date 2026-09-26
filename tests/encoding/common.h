//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of the encoding module share: the inputs the Go oracle
// makes the same way, the hash it keeps of a text, the codecs by the names
// the oracle gives them, and streams that hand out or take their bytes in
// pieces of a chosen size.
#pragma once

#include "tests/types.h"
#include "sgcl/encoding/encoding.h"

#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace enc_test {
    namespace io = sgcl::io;

    // splitmix64 seeded by the length, a byte of each output: the inputs
    // of tools/encoding_oracle.go
    inline std::vector<byte> input(size_t n) {
        uint64_t state = uint64_t(n) * 0x9E3779B97F4A7C15ull + 1;
        std::vector<byte> out(n);
        for (auto& b : out) {
            state += 0x9E3779B97F4A7C15ull;
            uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            z ^= z >> 31;
            b = byte(uint8_t(z >> 17));
        }
        return out;
    }

    inline uint64_t fnv(std::string_view s) {
        uint64_t h = 0xcbf29ce484222325ull;
        for (char c : s) {
            h ^= uint8_t(c);
            h *= 0x100000001b3ull;
        }
        return h;
    }

    inline std::vector<byte> from_hex(std::string_view h) {
        std::vector<byte> out;
        auto nibble = [](char c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; };
        for (size_t i = 0; i + 1 < h.size(); i += 2) {
            out.push_back(byte(uint8_t(nibble(h[i]) * 16 + nibble(h[i + 1]))));
        }
        return out;
    }

    inline std::vector<byte> bytes_of(const sgcl::vector<byte>& v) {
        return std::vector<byte>(v.begin(), v.end());
    }

    inline std::vector<byte> bytes_of(std::string_view s) {
        auto p = reinterpret_cast<const byte*>(s.data());
        return std::vector<byte>(p, p + s.size());
    }

    inline sgcl::slice<const byte> as_slice(const std::vector<byte>& v) {
        return sgcl::slice<const byte>(v.data(), v.size());
    }

    // A codec by the oracle's name, strict or lenient, as two functions
    struct AnyCodec {
        std::function<sgcl::string(sgcl::slice<const byte>)> encode;
        std::function<sgcl::expected<sgcl::vector<byte>, sgcl::encoding::error>(const sgcl::string&)> decode;
        bool base32 = false;
        bool padded = false;
    };

    template<class C>
    AnyCodec any_of(const C& c, bool b32) {
        AnyCodec a;
        a.encode = [c](sgcl::slice<const byte> d) { return c.encode(d); };
        a.decode = [c](const sgcl::string& t) { return c.decode(t); };
        a.base32 = b32;
        a.padded = c.padded();
        return a;
    }

    inline AnyCodec codec_named(std::string_view name, bool lenient) {
        using sgcl::encoding::base32;
        using sgcl::encoding::base64;
        auto pick64 = [&](const base64& c) { return any_of(lenient ? c.lenient() : c, false); };
        auto pick32 = [&](const base32& c) { return any_of(lenient ? c.lenient() : c, true); };
        if (name == "base64::standard") return pick64(base64::standard);
        if (name == "base64::url") return pick64(base64::url);
        if (name == "base64::raw_standard") return pick64(base64::raw_standard);
        if (name == "base64::raw_url") return pick64(base64::raw_url);
        if (name == "base32::standard") return pick32(base32::standard);
        if (name == "base32::hex") return pick32(base32::hex);
        if (name == "base32::standard.without_padding()") return pick32(base32::standard.without_padding());
        if (name == "base32::hex.without_padding()") return pick32(base32::hex.without_padding());
        if (name == "hex::encode" || name == "hex") {
            AnyCodec a;
            a.encode = [](sgcl::slice<const byte> d) { return sgcl::encoding::hex::encode(d); };
            a.decode = [](const sgcl::string& t) { return sgcl::encoding::hex::decode(t); };
            return a;
        }
        if (name == "ascii85::encode" || name == "ascii85") {
            AnyCodec a;
            a.encode = [](sgcl::slice<const byte> d) { return sgcl::encoding::ascii85::encode(d); };
            a.decode = [](const sgcl::string& t) { return sgcl::encoding::ascii85::decode(t); };
            return a;
        }
        throw std::invalid_argument(std::string(name));
    }

    // A reader that hands out its bytes in pieces of at most n
    class dribble final : public io::mixin::reader<dribble> {
    public:
        dribble(std::string s, size_t n) : _s(std::move(s)), _n(n) {}

        expected<size_t, io::error> read(sgcl::slice<byte> out) {
            size_t k = std::min({out.size(), _n, _s.size() - _at});
            std::memcpy(out.data(), _s.data() + _at, k);
            _at += k;
            return k;
        }

        sgcl::async::task<expected<size_t, io::error>> async_read(sgcl::slice<byte> out) {
            co_return read(out);
        }

    private:
        std::string _s;
        size_t _n;
        size_t _at = 0;
    };

    // A reader that fails after its bytes
    class failing final : public io::mixin::reader<failing> {
    public:
        explicit failing(std::string s) : _s(std::move(s)) {}

        expected<size_t, io::error> read(sgcl::slice<byte> out) {
            if (_at == _s.size()) {
                return sgcl::unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "failing"));
            }
            size_t k = std::min(out.size(), _s.size() - _at);
            std::memcpy(out.data(), _s.data() + _at, k);
            _at += k;
            return k;
        }

        sgcl::async::task<expected<size_t, io::error>> async_read(sgcl::slice<byte> out) {
            co_return read(out);
        }

    private:
        std::string _s;
        size_t _at = 0;
    };

    // A writer that keeps what it is given, counting the writes
    class sink final : public io::mixin::writer<sink> {
    public:
        using io::mixin::writer<sink>::write;
        using io::mixin::writer<sink>::async_write;

        expected<size_t, io::error> write(sgcl::slice<const byte> data) {
            text.append(reinterpret_cast<const char*>(data.data()), data.size());
            ++writes;
            return data.size();
        }

        sgcl::async::task<expected<size_t, io::error>> async_write(sgcl::slice<const byte> data) {
            co_return write(data);
        }

        std::string text;
        size_t writes = 0;
    };

    // A writer that fails every write
    class broken final : public io::mixin::writer<broken> {
    public:
        using io::mixin::writer<broken>::write;
        using io::mixin::writer<broken>::async_write;

        expected<size_t, io::error> write(sgcl::slice<const byte>) {
            return sgcl::unexpected(io::error(std::make_error_code(std::errc::broken_pipe), "write", "broken"));
        }

        sgcl::async::task<expected<size_t, io::error>> async_write(sgcl::slice<const byte> data) {
            co_return write(data);
        }
    };

    // A writer that fails its next `fail_next` writes and takes the rest
    class flaky final : public io::mixin::writer<flaky> {
    public:
        using io::mixin::writer<flaky>::write;
        using io::mixin::writer<flaky>::async_write;

        expected<size_t, io::error> write(sgcl::slice<const byte> data) {
            if (fail_next) {
                --fail_next;
                return sgcl::unexpected(io::error(std::make_error_code(std::errc::timed_out), "write", "flaky"));
            }
            text.append(reinterpret_cast<const char*>(data.data()), data.size());
            return data.size();
        }

        sgcl::async::task<expected<size_t, io::error>> async_write(sgcl::slice<const byte> data) {
            co_return write(data);
        }

        std::string text;
        int fail_next = 0;
    };

    // Everything a reader gives, read into buffers of `chunk` bytes: the
    // bytes, or the error that stopped it
    inline sgcl::expected<std::string, io::error> read_all(io::reader r, size_t chunk) {
        std::string out;
        std::vector<byte> buf(chunk);
        for (;;) {
            auto n = r.read(sgcl::slice<byte>(buf.data(), buf.size()));
            if (!n) {
                return sgcl::unexpected(n.error());
            }
            if (*n == 0) {
                return out;
            }
            out.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
    }

    inline std::string_view view_of(const std::vector<byte>& v) {
        return std::string_view(reinterpret_cast<const char*>(v.data()), v.size());
    }
}
