//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HPACK (sgcl/net/http/detail/h2/hpack.h) on any bytes. The first byte picks
// the mode:
//   even: the decoder on the rest as a sequence of blocks (a length byte
//     each), with a table size and a list limit from the input; nothing may
//     read past a block or crash (ASan, UBSan), a fault is a COMPRESSION_ERROR
//     of the connection, a block that decodes has its fields inside its
//     bytes, and the table never holds more than its limit;
//   odd: the encoder and the decoder, differentially: the rest read as
//     blocks of fields (and, now and then, a new table size for the peer),
//     each block encoded and decoded back: the same fields, in order, and
//     the two tables the same after each block, also when a small list
//     limit truncates it (the table must still be the encoder's).
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/h2_hpack_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/detail/h2/hpack.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {
    namespace h2 = sgcl::net::http::detail::h2;
    using sgcl::net::http::detail::HeadersAccess;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Input {
        const uint8_t* p;
        size_t n;
        size_t at = 0;

        bool more() const {
            return at < n;
        }

        uint8_t byte() {
            return at < n ? p[at++] : 0;
        }

        std::string take(size_t k) {
            k = std::min(k, n - at);
            std::string s(reinterpret_cast<const char*>(p + at), k);
            at += k;
            return s;
        }
    };

    sgcl::slice<const std::byte> view(const std::string& s) {
        return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }

    void same_tables(const h2::DynamicTable& a, const h2::DynamicTable& b) {
        check(a.count() == b.count() && a.size() == b.size());
        for (size_t i = 1; i <= a.count(); ++i) {
            check(a.entry(i).name == b.entry(i).name && a.entry(i).value == b.entry(i).value);
        }
    }

    void decoder_alone(Input& in) {
        const uint32_t table = uint32_t(in.byte()) * 32;
        const size_t limit = size_t(in.byte()) * 64 + 1;
        h2::Decoder d(table);
        while (in.more()) {
            if (in.byte() % 16 == 0) {
                d.set_max_table_size(uint32_t(in.byte()) * 32);
            }
            std::string block = in.take(in.byte());
            auto r = d.decode(view(block), limit);
            if (!r) {
                check(r.error().code == h2::ErrorCode::compression_error && r.error().connection() && r.error().what);
                return;   // the connection ends
            }
            check(d.table().size() <= d.table().limit());
            if (r->truncated) {
                check(HeadersAccess::fields(r->fields).empty());
                continue;
            }
            size_t list = 0;
            for (auto& f : HeadersAccess::fields(r->fields)) {
                const char* b = r->bytes.data();
                check(f.first.data() >= b && f.first.data() + f.first.size() <= b + r->bytes.size());
                check(f.second.data() >= b && f.second.data() + f.second.size() <= b + r->bytes.size());
                list += f.first.size() + f.second.size() + 32;
            }
            check(list <= limit);
        }
    }

    void differential(Input& in) {
        const uint8_t flags = in.byte();
        const auto huffman = flags & 1 ? h2::Encoder::Huffman::shorter : (flags & 2 ? h2::Encoder::Huffman::always : h2::Encoder::Huffman::never);
        const size_t limit = flags & 4 ? size_t(in.byte()) * 16 + 1 : (1u << 20);
        h2::Encoder e(4096, huffman);
        h2::Decoder d(4096);
        while (in.more()) {
            const uint8_t op = in.byte();
            if (op % 8 == 0) {
                e.set_peer_max_table_size(uint32_t(in.byte()) * 17);   // up to 4335: the encoder keeps at most its own 4096
            }
            std::string block;
            e.begin_block(block);
            std::vector<std::pair<std::string, std::string>> fields;
            const int n = op % 9;
            size_t list = 0;
            for (int i = 0; i < n && in.more(); ++i) {
                const uint8_t kind = in.byte();
                std::string name;
                if (kind % 5 == 0) {
                    name = std::string(h2::static_table[in.byte() % 61].name);   // a name of the static table
                } else if (kind % 5 == 1) {
                    name = "cookie";
                } else {
                    name = in.take(in.byte() % 24);
                }
                std::string value = kind % 3 == 0 ? std::string(h2::static_table[in.byte() % 61].value) : in.take(in.byte());
                e.encode(block, name, value);
                list += name.size() + value.size() + 32;
                fields.emplace_back(std::move(name), std::move(value));
            }
            auto r = d.decode(view(block), limit);
            check(r.has_value());
            check(r->truncated == (list > limit));
            if (!r->truncated) {
                auto& got = HeadersAccess::fields(r->fields);
                check(got.size() == fields.size());
                for (size_t i = 0; i < fields.size(); ++i) {
                    check(got[i].first.view() == fields[i].first && got[i].second.view() == fields[i].second);
                }
            }
            same_tables(e.table(), d.table());
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    Input in{data + 1, size - 1};
    if (data[0] & 1) {
        differential(in);
    } else {
        decoder_alone(in);
    }
    return 0;
}
