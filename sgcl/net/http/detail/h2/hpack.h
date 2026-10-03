//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "hpack_tables.h"
#include "../../headers.h"
#include "../../../../core/aliases.h"
#include "../../../../core/detail/bytes.h"
#include "../../../../core/expected.h"
#include "../../../../core/slice.h"
#include "../../../../core/string.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// HPACK (RFC 7541), the header compression of HTTP/2: the primitives here
// (the integers of §5.1, the Huffman code of §5.2 and Appendix B), the
// tables, the decoder and the encoder over them (below). Written from the
// RFC; its Appendix C is the test's oracle, byte for byte both ways
// (tests/net/http/hpack.cpp).
//
// Nothing here allocates on its own: the primitives read from a view and
// append to a buffer the caller keeps (a std::string whose capacity stays
// with the connection, as Wire::out does), so that a request's headers
// cost the system's allocator nothing.
namespace sgcl::net::http::detail::h2 {
    // What went wrong in a block: each is a COMPRESSION_ERROR of the
    // connection (RFC 9113 §4.3); `hpack_fault_text` words it
    enum class HpackFault : uint8_t {
        none,
        truncated,              // a representation cut off at the end of the block
        integer_overflow,       // an integer past 2^32 - 1
        string_past_block,      // a string's length past the end of the block
        huffman_padding,        // padding longer than 7 bits, or not a prefix of EOS (not all ones)
        huffman_eos,            // the EOS symbol inside a string
        index_zero,             // index 0
        index_past_table,       // an index past the static and the dynamic table
        table_size_past_limit,  // a Dynamic Table Size Update past SETTINGS_HEADER_TABLE_SIZE
        table_size_misplaced,   // a Dynamic Table Size Update after a field, or a third one
        table_size_missing,     // no Dynamic Table Size Update where SETTINGS lowered the limit
    };

    inline const char* hpack_fault_text(HpackFault f) noexcept {
        switch (f) {
            case HpackFault::none: return "no fault";
            case HpackFault::truncated: return "HPACK: a representation cut off at the end of the block";
            case HpackFault::integer_overflow: return "HPACK: an integer past 2^32 - 1";
            case HpackFault::string_past_block: return "HPACK: a string past the end of the block";
            case HpackFault::huffman_padding: return "HPACK: Huffman padding longer than 7 bits or not all ones";
            case HpackFault::huffman_eos: return "HPACK: the EOS symbol inside a Huffman string";
            case HpackFault::index_zero: return "HPACK: index 0";
            case HpackFault::index_past_table: return "HPACK: an index past the tables";
            case HpackFault::table_size_past_limit: return "HPACK: a table size update past SETTINGS_HEADER_TABLE_SIZE";
            case HpackFault::table_size_misplaced: return "HPACK: a table size update after a field or a third one";
            case HpackFault::table_size_missing: return "HPACK: no table size update after SETTINGS lowered the table";
        }
        return "HPACK: a fault";
    }

    // --- §5.1 integers --------------------------------------------------------

    // `value` with an N-bit prefix, the first octet's other bits `first`
    // (the representation's pattern, its low N bits zero)
    inline void put_integer(std::string& out, uint8_t first, int prefix, uint64_t value) noexcept {
        const uint64_t max = (uint64_t(1) << prefix) - 1;
        if (value < max) {
            out.push_back(char(first | uint8_t(value)));
            return;
        }
        out.push_back(char(first | uint8_t(max)));
        value -= max;
        while (value >= 128) {
            out.push_back(char(uint8_t(value % 128 + 128)));
            value /= 128;
        }
        out.push_back(char(uint8_t(value)));
    }

    // An integer with an N-bit prefix at p (the prefix in *p's low bits),
    // p moved past it; at most 2^32 - 1, as the values of HTTP/2 are
    inline HpackFault get_integer(const uint8_t*& p, const uint8_t* end, int prefix, uint32_t& value) noexcept {
        if (p == end) {
            return HpackFault::truncated;
        }
        const uint32_t max = (uint32_t(1) << prefix) - 1;
        uint64_t v = *p++ & max;
        if (v < max) {
            value = uint32_t(v);
            return HpackFault::none;
        }
        for (int shift = 0;; shift += 7) {
            if (p == end) {
                return HpackFault::truncated;
            }
            const uint8_t b = *p++;
            if (shift > 28) {
                return HpackFault::integer_overflow;   // a fifth continuation octet: past 2^35, or zeros padding it (not produced by any encoder)
            }
            v += uint64_t(b & 127) << shift;
            if (v > 0xFFFFFFFFu) {
                return HpackFault::integer_overflow;
            }
            if (!(b & 128)) {
                break;
            }
        }
        value = uint32_t(v);
        return HpackFault::none;
    }

    // --- §5.2 and Appendix B: the Huffman code --------------------------------

    namespace huffman_detail {
        // The decoder's tables, made at compile time from Appendix B: the
        // codes of at most 8 bits by their first octet (a code padded with
        // every tail), and for longer ones the canonical ranges: for each
        // length its first code, how many there are and where their
        // symbols begin in the list ordered by (length, code)
        struct Short {
            uint16_t symbol = 0;
            uint8_t bits = 0;   // 0: no code of 8 bits or fewer begins so
        };

        struct Tables {
            std::array<Short, 256> first_octet{};
            std::array<uint32_t, 31> first_code{};
            std::array<uint16_t, 31> count{};
            std::array<uint16_t, 31> start{};
            std::array<uint16_t, 257> by_code{};
            bool canonical = true;   // codes of one length consecutive (checked by the test)
        };

        consteval Tables make_tables() {
            Tables t;
            // symbols ordered by (bits, code)
            std::array<uint16_t, 257> order{};
            for (uint16_t s = 0; s < 257; ++s) {
                order[s] = s;
            }
            for (size_t i = 1; i < 257; ++i) {
                for (size_t j = i; j > 0; --j) {
                    auto a = huffman_codes[order[j - 1]];
                    auto b = huffman_codes[order[j]];
                    if (a.bits < b.bits || (a.bits == b.bits && a.code <= b.code)) {
                        break;
                    }
                    auto x = order[j - 1];
                    order[j - 1] = order[j];
                    order[j] = x;
                }
            }
            t.by_code = order;
            for (size_t i = 0; i < 257; ++i) {
                auto c = huffman_codes[order[i]];
                if (t.count[c.bits] == 0) {
                    t.first_code[c.bits] = c.code;
                    t.start[c.bits] = uint16_t(i);
                } else if (c.code != t.first_code[c.bits] + t.count[c.bits]) {
                    t.canonical = false;
                }
                ++t.count[c.bits];
                if (c.bits <= 8) {
                    const unsigned free = 8 - c.bits;
                    for (unsigned tail = 0; tail < (1u << free); ++tail) {
                        t.first_octet[(c.code << free) | tail] = Short{order[i], c.bits};
                    }
                }
            }
            return t;
        }

        inline constexpr Tables tables = make_tables();
        static_assert(tables.canonical, "RFC 7541 Appendix B: the codes of one length are consecutive");

        // The symbol of the code of `bits` bits at the top of the `have`
        // bits of `acc`, or -1
        inline int long_symbol(uint64_t acc, int have, int bits) noexcept {
            const uint32_t code = uint32_t(acc >> (have - bits)) & ((uint32_t(1) << bits) - 1);
            const uint32_t k = code - tables.first_code[size_t(bits)];
            if (code >= tables.first_code[size_t(bits)] && k < tables.count[size_t(bits)]) {
                return tables.by_code[tables.start[size_t(bits)] + k];
            }
            return -1;
        }
    }

    // The bytes the Huffman code of s takes
    inline size_t huffman_length(std::string_view s) noexcept {
        uint64_t bits = 0;
        for (unsigned char c : s) {
            bits += huffman_codes[c].bits;
        }
        return size_t((bits + 7) / 8);
    }

    // s in the Huffman code, padded with the ones of EOS's prefix
    inline void huffman_encode(std::string& out, std::string_view s) noexcept {
        uint64_t acc = 0;
        int have = 0;
        for (unsigned char c : s) {
            const auto code = huffman_codes[c];
            acc = (acc << code.bits) | code.code;
            have += code.bits;
            while (have >= 8) {
                have -= 8;
                out.push_back(char(uint8_t(acc >> have)));
            }
            acc &= (uint64_t(1) << have) - 1;
        }
        if (have > 0) {
            out.push_back(char(uint8_t((acc << (8 - have)) | ((1u << (8 - have)) - 1))));
        }
    }

    // The Huffman string at [p, p + n) decoded onto the end of `out`; a
    // fault for EOS inside it or a padding that is not at most 7 ones
    inline HpackFault huffman_decode(std::string& out, const uint8_t* p, size_t n) noexcept {
        using namespace huffman_detail;
        uint64_t acc = 0;
        int have = 0;   // bits in acc, at most 37
        const uint8_t* end = p + n;
        for (;;) {
            // a symbol from the bits there, as many as they hold
            for (;;) {
                if (have >= 8) {
                    const auto s = tables.first_octet[size_t(acc >> (have - 8)) & 255];
                    if (s.bits) {
                        out.push_back(char(uint8_t(s.symbol)));
                        have -= s.bits;
                        acc &= (uint64_t(1) << have) - 1;
                        continue;
                    }
                    int symbol = -1;
                    int bits = 9;
                    for (; bits <= 30 && bits <= have; ++bits) {
                        symbol = long_symbol(acc, have, bits);
                        if (symbol >= 0) {
                            break;
                        }
                    }
                    if (symbol < 0) {
                        if (have >= 30) {
                            return HpackFault::huffman_padding;   // 30 bits and no code: cannot be (every 30-bit string holds a code's prefix... or EOS's)
                        }
                        break;   // a longer code: more bytes
                    }
                    if (symbol == 256) {
                        return HpackFault::huffman_eos;
                    }
                    out.push_back(char(uint8_t(symbol)));
                    have -= bits;
                    acc &= (uint64_t(1) << have) - 1;
                    continue;
                }
                if (p == end && have >= 5) {
                    // the last bits: a short code whole in them
                    int symbol = -1;
                    int bits = 5;
                    for (; bits <= have; ++bits) {
                        const uint32_t code = uint32_t(acc >> (have - bits)) & ((uint32_t(1) << bits) - 1);
                        const uint32_t k = code - tables.first_code[size_t(bits)];
                        if (code >= tables.first_code[size_t(bits)] && k < tables.count[size_t(bits)]) {
                            symbol = tables.by_code[tables.start[size_t(bits)] + k];
                            break;
                        }
                    }
                    if (symbol >= 0) {
                        out.push_back(char(uint8_t(symbol)));
                        have -= bits;
                        acc &= (uint64_t(1) << have) - 1;
                        continue;
                    }
                }
                break;
            }
            if (p == end) {
                break;
            }
            acc = (acc << 8) | *p++;
            have += 8;
        }
        // what is left: at most 7 bits, all ones (a prefix of EOS)
        if (have > 7 || acc != (uint64_t(1) << have) - 1) {
            return HpackFault::huffman_padding;
        }
        return HpackFault::none;
    }

    // --- §2.3 and §4: the dynamic table ------------------------------------------

    // The dynamic table of one side of a connection: its entries' bytes in
    // one block and their places in a ring, newest first to the reader
    // (index 1), both unmanaged and made once for the largest size the
    // table may have (SETTINGS_HEADER_TABLE_SIZE), so that an entry costs no
    // allocation. An entry's size is its name, its value and 32 (§4.1).
    class DynamicTable {
    public:
        explicit DynamicTable(uint32_t max_size) noexcept {
            reset_capacity(max_size);
        }

        DynamicTable(const DynamicTable&) = delete;
        DynamicTable& operator=(const DynamicTable&) = delete;

        // A new largest size (a SETTINGS): the blocks made again, what
        // fits kept
        void reset_capacity(uint32_t max_size) noexcept {
            std::vector<std::pair<std::string, std::string>> keep;   // once per SETTINGS, not per field
            for (size_t i = _count; i > 0; --i) {
                auto e = entry(i);
                keep.emplace_back(std::string(e.name), std::string(e.value));
            }
            _capacity = max_size;
            _slots = size_t(max_size) / 32 + 1;
            _entries = std::make_unique<Slot[]>(_slots);
            _bytes = std::make_unique<char[]>(size_t(max_size) * 2 + 1);
            _head = _count = 0;
            _used = 0;
            _end = 0;
            if (_limit > max_size) {
                _limit = max_size;
            }
            for (auto& [n, v] : keep) {
                add(n, v);
            }
        }

        uint32_t capacity() const noexcept {
            return _capacity;
        }

        // The size the table is held to now (a Dynamic Table Size Update),
        // at most the capacity: entries evicted to fit
        void set_limit(uint32_t n) noexcept {
            _limit = n;
            _evict_to(n);
        }

        uint32_t limit() const noexcept {
            return _limit;
        }

        size_t size() const noexcept {
            return _used;
        }

        size_t count() const noexcept {
            return _count;
        }

        struct View {
            std::string_view name;
            std::string_view value;
        };

        // Entry i, 1 the newest
        View entry(size_t i) const noexcept {
            const Slot& s = _entries[(_head + _slots - i) % _slots];
            return View{std::string_view(_bytes.get() + s.at, s.name), std::string_view(_bytes.get() + s.at + s.name, s.value)};
        }

        // A new entry (§4.4): the oldest evicted until it fits; one larger
        // than the table empties it and is not added. name and value must
        // not be views of this table (the caller's copies)
        void add(std::string_view name, std::string_view value) noexcept {
            const size_t size = name.size() + value.size() + 32;
            if (size > _limit) {
                _evict_to(0);
                return;
            }
            _evict_to(_limit - size);
            const size_t n = name.size() + value.size();
            if (_end + n > size_t(_capacity) * 2 + 1) {
                _compact();
            }
            sgcl::detail::copy_bytes(_bytes.get() + _end, name.data(), name.size());
            sgcl::detail::copy_bytes(_bytes.get() + _end + name.size(), value.data(), value.size());
            _entries[_head] = Slot{uint32_t(_end), uint32_t(name.size()), uint32_t(value.size())};
            _head = (_head + 1) % _slots;
            ++_count;
            _end += n;
            _used += size;
        }

    private:
        struct Slot {
            uint32_t at = 0;
            uint32_t name = 0;
            uint32_t value = 0;
        };

        void _evict_to(size_t n) noexcept {
            while (_used > n && _count > 0) {
                const Slot& s = _entries[(_head + _slots - _count) % _slots];
                _used -= size_t(s.name) + s.value + 32;
                --_count;
            }
            if (_count == 0) {
                _end = 0;
            }
        }

        // The live entries' bytes moved to the front of the block, oldest
        // first (the block holds twice the capacity: a move every capacity
        // bytes added at most)
        void _compact() noexcept {
            size_t to = 0;
            for (size_t i = _count; i > 0; --i) {
                Slot& s = _entries[(_head + _slots - i) % _slots];
                const size_t n = size_t(s.name) + s.value;
                std::memmove(_bytes.get() + to, _bytes.get() + s.at, n);
                s.at = uint32_t(to);
                to += n;
            }
            _end = to;
        }

        uint32_t _capacity = 0;     // SETTINGS_HEADER_TABLE_SIZE
        uint32_t _limit = 4096;     // the size in force (Dynamic Table Size Update)
        size_t _slots = 0;
        std::unique_ptr<Slot[]> _entries;
        std::unique_ptr<char[]> _bytes;
        size_t _head = 0;           // the slot the next entry takes
        size_t _count = 0;
        size_t _used = 0;           // §4.1 size
        size_t _end = 0;            // the end of the bytes in use
    };

    // --- §3 decoding -------------------------------------------------------------

    // A block decoded: one managed string with every name and value in the
    // order of the block, and the fields as slices of it (pseudo-fields
    // too); `truncated` when the list went past the limit, its fields then
    // not kept (the server answers 431; the table is as the peer's)
    struct Block {
        string bytes;
        headers fields;
        bool truncated = false;
    };

    // The decoder of one connection (the peer's encoder's twin): its table
    // and the scratch a block is decoded into, unmanaged, kept from block
    // to block; a block then costs one managed string and the fields'
    // array, as a head of HTTP/1.1 does
    class Decoder {
    public:
        // our SETTINGS_HEADER_TABLE_SIZE: the most a Dynamic Table Size
        // Update may ask
        explicit Decoder(uint32_t max_table_size = 4096) noexcept
        : _table(max_table_size), _max(max_table_size) {
            _table.set_limit(max_table_size);
        }

        // Our new SETTINGS_HEADER_TABLE_SIZE, once the peer acknowledged
        // it: a lower one must be met by a Dynamic Table Size Update at the
        // start of the next block (§4.2)
        void set_max_table_size(uint32_t n) noexcept {
            if (n < _max) {
                _update_required = true;
            }
            _max = n;
            if (n > _table.capacity()) {
                _table.reset_capacity(n);
            }
            if (_table.limit() > n) {
                _table.set_limit(n);
            }
        }

        const DynamicTable& table() const noexcept {
            return _table;
        }

        // One whole header block (HEADERS and its CONTINUATIONs, joined by
        // the connection); max_list_size counted as §4.1 (name + value + 32
        // a field)
        expected<Block, Error> decode(const slice<const byte>& block, size_t max_list_size) noexcept {
            _scratch.clear();
            _refs.clear();
            const uint8_t* p = reinterpret_cast<const uint8_t*>(block.data());
            const uint8_t* end = p + block.size();
            bool fields_begun = false;
            int updates = 0;
            size_t list = 0;
            bool truncated = false;
            while (p < end) {
                const uint8_t b = *p;
                HpackFault f = HpackFault::none;
                if (b & 0x80) {   // §6.1 indexed
                    uint32_t index;
                    if ((f = get_integer(p, end, 7, index)) != HpackFault::none) {
                        return _fail(f);
                    }
                    View v;
                    if ((f = _lookup(index, v)) != HpackFault::none) {
                        return _fail(f);
                    }
                    fields_begun = true;
                    _emit(v.name, v.value, max_list_size, list, truncated);
                    continue;
                }
                if ((b & 0xe0) == 0x20) {   // §6.3 Dynamic Table Size Update
                    uint32_t n;
                    if ((f = get_integer(p, end, 5, n)) != HpackFault::none) {
                        return _fail(f);
                    }
                    if (fields_begun || ++updates > 2) {
                        return _fail(HpackFault::table_size_misplaced);
                    }
                    if (n > _max) {
                        return _fail(HpackFault::table_size_past_limit);
                    }
                    _table.set_limit(n);
                    _update_required = false;
                    continue;
                }
                if (_update_required) {
                    return _fail(HpackFault::table_size_missing);
                }
                // §6.2 literals: with incremental indexing (01, 6 bits),
                // without (0000, 4 bits), never indexed (0001, 4 bits)
                const bool indexing = (b & 0xc0) == 0x40;
                const int prefix = indexing ? 6 : 4;
                uint32_t index;
                if ((f = get_integer(p, end, prefix, index)) != HpackFault::none) {
                    return _fail(f);
                }
                fields_begun = true;
                // the name and the value into the scratch (the table's
                // entries may be evicted by the add below: copied first)
                const size_t mark = _scratch.size();
                if (index) {
                    View v;
                    if ((f = _lookup(index, v)) != HpackFault::none) {
                        return _fail(f);
                    }
                    _scratch.append(v.name);
                } else if ((f = _string(p, end)) != HpackFault::none) {
                    return _fail(f);
                }
                const size_t name_end = _scratch.size();
                if ((f = _string(p, end)) != HpackFault::none) {
                    return _fail(f);
                }
                std::string_view name(_scratch.data() + mark, name_end - mark);
                std::string_view value(_scratch.data() + name_end, _scratch.size() - name_end);
                if (indexing) {
                    _table.add(name, value);
                }
                _account(mark, name.size(), value.size(), max_list_size, list, truncated);
            }
            if (_update_required) {
                return _fail(HpackFault::table_size_missing);   // a block of nothing but no update: still owed
            }
            Block out;
            out.truncated = truncated;
            if (!truncated) {
                out.bytes = string(std::string_view(_scratch));
                auto& fields = HeadersAccess::fields(out.fields);
                fields.reserve(_refs.size());
                for (auto& r : _refs) {
                    HeadersAccess::add(out.fields, out.bytes.as_slice(r.at, r.name), out.bytes.as_slice(r.at + r.name, r.value));
                }
            }
            return out;
        }

    private:
        using View = DynamicTable::View;

        struct Ref {
            size_t at, name, value;
        };

        static unexpected<Error> _fail(HpackFault f) noexcept {
            return unexpected(connection_error(ErrorCode::compression_error, hpack_fault_text(f)));
        }

        // Index i of the address space of §2.3.3: the static table, then
        // the dynamic one
        HpackFault _lookup(uint32_t index, View& v) const noexcept {
            if (index == 0) {
                return HpackFault::index_zero;
            }
            if (index <= 61) {
                v = View{static_table[index - 1].name, static_table[index - 1].value};
                return HpackFault::none;
            }
            if (index - 61 > _table.count()) {
                return HpackFault::index_past_table;
            }
            v = _table.entry(index - 61);
            return HpackFault::none;
        }

        // A string literal (§5.2) onto the end of the scratch
        HpackFault _string(const uint8_t*& p, const uint8_t* end) noexcept {
            if (p == end) {
                return HpackFault::truncated;
            }
            const bool huffman = *p & 0x80;
            uint32_t n;
            if (auto f = get_integer(p, end, 7, n); f != HpackFault::none) {
                return f;
            }
            if (n > size_t(end - p)) {
                return HpackFault::string_past_block;
            }
            if (huffman) {
                if (auto f = huffman_decode(_scratch, p, n); f != HpackFault::none) {
                    return f;
                }
            } else {
                _scratch.append(reinterpret_cast<const char*>(p), n);
            }
            p += n;
            return HpackFault::none;
        }

        // An indexed field: its name and value copied into the scratch
        void _emit(std::string_view name, std::string_view value, size_t max, size_t& list, bool& truncated) noexcept {
            const size_t mark = _scratch.size();
            list += name.size() + value.size() + 32;
            if (truncated || list > max) {
                truncated = true;
                return;
            }
            _scratch.append(name);
            _scratch.append(value);
            _refs.push_back(Ref{mark, name.size(), value.size()});
        }

        // A literal decoded at mark: kept, or dropped once the list is past
        // the limit (the scratch given back, so it never holds more than
        // the limit and one field)
        void _account(size_t mark, size_t name, size_t value, size_t max, size_t& list, bool& truncated) noexcept {
            list += name + value + 32;
            if (truncated || list > max) {
                truncated = true;
                _scratch.resize(mark);
                return;
            }
            _refs.push_back(Ref{mark, name, value});
        }

        DynamicTable _table;
        uint32_t _max;
        bool _update_required = false;
        std::string _scratch;          // the block's names and values; capacity kept
        std::vector<Ref> _refs;        // capacity kept
    };

    // --- §6 encoding -------------------------------------------------------------

    // The encoder of one connection: its table as the peer's decoder keeps
    // it. Strategy (as Go's): an exact match in a table is an index; the
    // fields that must not reach a table (authorization,
    // proxy-authorization, cookie: §7.1.3) are literals never indexed; a
    // field larger than 3/4 of the table a literal without indexing; any
    // other a literal with incremental indexing, its name an index when a
    // table has it. A string is Huffman coded when that is strictly shorter
    // (Huffman::shorter); never or always as the examples of RFC 7541
    // Appendix C do (C.3 and C.5; C.4 and C.6), for their test.
    class Encoder {
    public:
        enum class Huffman : uint8_t { shorter, never, always };

        // the table we use, at most the peer's SETTINGS_HEADER_TABLE_SIZE
        // (4096 until its SETTINGS say otherwise)
        explicit Encoder(uint32_t table_size = 4096, Huffman huffman = Huffman::shorter) noexcept
        : _table(table_size), _want(table_size), _huffman(huffman) {
            _table.set_limit(table_size);
        }

        // The peer's SETTINGS_HEADER_TABLE_SIZE: the table held to the
        // smaller of it and ours, a Dynamic Table Size Update at the start
        // of the next block (the smallest size in between first, §4.2)
        void set_peer_max_table_size(uint32_t n) noexcept {
            const uint32_t size = n < _want ? n : _want;
            if (size > _table.capacity()) {
                _table.reset_capacity(size);
            }
            _pending_min = _pending ? (size < _pending_min ? size : _pending_min) : size;
            _pending = true;
            _pending_final = size;
            _table.set_limit(size);
        }

        const DynamicTable& table() const noexcept {
            return _table;
        }

        // The start of a block: the updates owed
        void begin_block(std::string& out) noexcept {
            if (!_pending) {
                return;
            }
            if (_pending_min < _pending_final) {
                put_integer(out, 0x20, 5, _pending_min);
            }
            put_integer(out, 0x20, 5, _pending_final);
            _pending = false;
        }

        void encode(std::string& out, std::string_view name, std::string_view value) noexcept {
            size_t name_index = 0;
            if (size_t exact = _find(name, value, name_index)) {
                put_integer(out, 0x80, 7, exact);
                return;
            }
            if (_sensitive(name)) {
                _literal(out, 0x10, 4, name_index, name, value);
                return;
            }
            const size_t size = name.size() + value.size() + 32;
            if (size * 4 > size_t(_table.limit()) * 3) {
                _literal(out, 0x00, 4, name_index, name, value);
                return;
            }
            _literal(out, 0x40, 6, name_index, name, value);
            _table.add(name, value);
        }

    private:
        static bool _sensitive(std::string_view name) noexcept {
            return name == "authorization" || name == "proxy-authorization" || name == "cookie";
        }

        // The index of name and value in a table (0: none), and of the
        // name alone in `name_index`
        size_t _find(std::string_view name, std::string_view value, size_t& name_index) const noexcept {
            for (size_t i = 0; i < 61; ++i) {
                if (static_table[i].name == name) {
                    if (static_table[i].value == value) {
                        return i + 1;
                    }
                    if (!name_index) {
                        name_index = i + 1;
                    }
                }
            }
            for (size_t i = 1; i <= _table.count(); ++i) {
                auto e = _table.entry(i);
                if (e.name == name) {
                    if (e.value == value) {
                        return 61 + i;
                    }
                    if (!name_index) {
                        name_index = 61 + i;
                    }
                }
            }
            return 0;
        }

        void _literal(std::string& out, uint8_t pattern, int prefix, size_t name_index, std::string_view name, std::string_view value) noexcept {
            put_integer(out, pattern, prefix, name_index);
            if (!name_index) {
                _string(out, name);
            }
            _string(out, value);
        }

        void _string(std::string& out, std::string_view s) noexcept {
            if (_huffman != Huffman::never) {
                const size_t h = huffman_length(s);
                if (h < s.size() || _huffman == Huffman::always) {
                    put_integer(out, 0x80, 7, h);
                    huffman_encode(out, s);
                    return;
                }
            }
            put_integer(out, 0x00, 7, s.size());
            out.append(s);
        }

        DynamicTable _table;
        uint32_t _want;
        Huffman _huffman;
        bool _pending = false;
        uint32_t _pending_min = 0;
        uint32_t _pending_final = 0;
    };
}
