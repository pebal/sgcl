//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The codecs of the encoding module on any bytes, without an oracle: base64
// (the four alphabets, strict and lenient), base32 (both alphabets, padded
// and not), ascii85, hex, PEM, CSV and varint. The first byte picks the
// codec and its options, the last the size of the pieces a stream is fed
// in. What must hold:
//   - bytes encoded decode to themselves, in memory, into a buffer of
//     max_decoded_size and through the stream decoder fed in pieces; the
//     stream encoder written in pieces writes what encode writes;
//   - a text a strict codec decodes is the text its bytes encode to (the
//     strict decoding is one to one); one a lenient codec decodes decodes
//     again to the same bytes once encoded; the stream decoder takes what
//     the decoder takes, with the same bytes, and refuses what it refuses;
//   - a PEM block parsed, written and parsed again is the same block (and
//     writes the same text again), and so is every block of parse_all;
//   - CSV read in pieces from a stream gives the records (or the error)
//     read from the text in one; the records written and read again are
//     as many, each with as many fields (none lost: a record of one empty
//     field among them), and writing comes to a text it writes again
//     (a field's "\r\n" comes back "\n", as in Go, a "\r" a round);
//   - a varint read is the number it encodes, whose shortest encoding
//     reads to it again, from the bytes and from a stream alike.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/codecs_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // A reader handing out its bytes in pieces of at most n
    class dribble final : public io::mixin::reader<dribble> {
    public:
        dribble(std::string s, size_t n) : _s(std::move(s)), _n(n) {}

        expected<size_t, io::error> read(slice<byte> out) {
            size_t k = std::min({out.size(), _n, _s.size() - _at});
            std::memcpy(out.data(), _s.data() + _at, k);
            _at += k;
            return k;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

    private:
        std::string _s;
        size_t _n;
        size_t _at = 0;
    };

    class sink final : public io::mixin::writer<sink> {
    public:
        using io::mixin::writer<sink>::write;
        using io::mixin::writer<sink>::async_write;

        expected<size_t, io::error> write(slice<const byte> data) {
            text.append(reinterpret_cast<const char*>(data.data()), data.size());
            return data.size();
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            co_return write(data);
        }

        std::string text;
    };

    std::string_view view(const vector<byte>& v) {
        return std::string_view(reinterpret_cast<const char*>(v.data()), v.size());
    }

    expected<std::string, io::error> read_all(io::reader r, size_t chunk) {
        std::string out;
        std::vector<byte> buf(chunk);
        for (;;) {
            auto n = r.read(slice<byte>(buf.data(), buf.size()));
            if (!n) {
                return unexpected(n.error());
            }
            if (*n == 0) {
                return out;
            }
            out.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
    }

    template<class Codec>
    void codec(const Codec& c, bool strict, bool upper_or_lower, std::string_view in, size_t piece) {
        slice<const byte> bytes(reinterpret_cast<const byte*>(in.data()), in.size());
        // bytes encoded decode to themselves
        string text = c.encode(bytes);
        auto back = c.decode(text);
        check(back.has_value() && view(*back) == in);
        {
            std::vector<byte> room(c.max_decoded_size(text.size()) + 1);
            auto n = c.decode_to(slice<byte>(room.data(), room.size()), text);
            check(n.has_value() && std::string_view(reinterpret_cast<const char*>(room.data()), *n) == in);
        }
        {
            tracked_ptr out = make_tracked<sink>();
            auto enc = c.encoder_to(out);
            for (size_t at = 0; at < in.size(); at += piece) {
                size_t k = std::min(piece, in.size() - at);
                auto w = enc.write(slice<const byte>(bytes.data() + at, k));
                check(w.has_value() && *w == k);
            }
            check(enc.close().has_value());
            check(out->text == text.view());
            auto dec = c.decoder_from(make_tracked<dribble>(std::string(text.view()), piece));
            auto all = read_all(dec, 1 + piece % 7);
            check(all.has_value() && *all == in);
        }
        // the input as a text to decode
        string given(in);
        auto d = c.decode(given);
        auto dec = c.decoder_from(make_tracked<dribble>(std::string(in), piece));
        auto streamed = read_all(dec, 1 + piece % 5);
        check(d.has_value() == streamed.has_value());
        if (!d) {
            check(d.error().offset() <= in.size());
            return;
        }
        check(*streamed == view(*d));
        string again = c.encode(*d);
        if (strict) {
            if (upper_or_lower) {
                check(again.size() == given.size());
                for (size_t i = 0; i < again.size(); ++i) {
                    check((again.view()[i] | 0x20) == (given.view()[i] | 0x20));
                }
            } else {
                check(again == given);
            }
        }
        auto twice = c.decode(again);
        check(twice.has_value() && view(*twice) == view(*d));
    }

    void pem_blocks(std::string_view in) {
        string text(in);
        auto one = pem::parse(text);
        if (one) {
            string written = one->to_string();
            auto again = pem::parse(written);
            check(again.has_value());
            check(again->type() == one->type());
            check(view(again->bytes()) == view(one->bytes()));
            // the same headers; in their order but for Proc-Type, which
            // the writing puts first (RFC 1421)
            check(again->headers().size() == one->headers().size());
            for (auto& b : one->headers()) {
                bool found = false;
                for (auto& a : again->headers()) {
                    found = found || (a.first == b.first && a.second == b.second);
                }
                check(found);
            }
            check(again->to_string() == written);
        }
        auto all = pem::parse_all(text);
        if (all) {
            std::string written;
            for (auto& b : *all) {
                written += b.to_string().view();
            }
            auto again = pem::parse_all(string(written));
            check(again.has_value() && again->size() == all->size());
            for (size_t i = 0; i < all->size(); ++i) {
                check((*again)[i].to_string() == (*all)[i].to_string());
            }
        }
    }

    struct Records {
        std::vector<std::vector<std::string>> rows;
        bool failed = false;
        encoding::errc code{};
        uint64_t offset = 0;
    };

    Records records(csv::reader& r) {
        Records out;
        while (auto row = r.next()) {
            std::vector<std::string> fields;
            for (auto f : *row) {
                fields.emplace_back(f.view());
            }
            out.rows.push_back(std::move(fields));
            check(out.rows.size() < 100000);
        }
        if (r.last_error()) {
            out.failed = true;
            out.code = r.last_error()->code();
            out.offset = r.last_error()->offset();
        }
        return out;
    }

    std::string written(const Records& rs, const csv::options& o) {
        tracked_ptr out = make_tracked<sink>();
        csv::writer w(out, o);
        for (auto& row : rs.rows) {
            w.write(row);
        }
        check(w.flush().has_value());
        return out->text;
    }

    void csv_text(uint8_t mode, std::string_view in, size_t piece) {
        static const char separators[] = {',', ';', '\t', '|', ' ', 'a', '#'};
        csv::options o;
        o.separator = separators[mode % 7];
        o.trim_leading_space = (mode >> 3) & 1;
        o.lazy_quotes = (mode >> 4) & 1;
        o.same_field_count = (mode >> 5) & 1;
        o.comment = (mode >> 6) & 1 ? '#' : 0;
        if (o.comment == o.separator) {
            o.comment = 0;
        }
        csv::reader whole(string(in), o);
        Records a = records(whole);
        csv::reader pieces(make_tracked<dribble>(std::string(in), piece), o);
        Records b = records(pieces);
        check(a.rows == b.rows);
        check(a.failed == b.failed);
        if (a.failed) {
            check(a.code == b.code && a.offset == b.offset);
            return;
        }
        // written and read again: the records; written again: the same text
        csv::options plain;
        plain.separator = o.separator;
        plain.same_field_count = false;
        // a record is never lost nor split, its fields as many; a field's
        // "\r\n" is read back "\n" (as Go's writer and reader do), so the
        // text and the records are stable from the second writing on
        std::string first = written(a, plain);
        csv::reader r(string(first), plain);
        Records c = records(r);
        check(!c.failed);
        check(c.rows.size() == a.rows.size());
        for (size_t i = 0; i < c.rows.size(); ++i) {
            check(c.rows[i].size() == a.rows[i].size());
        }
        // each round takes one "\r" before a "\n" off a field ("\r\r\n"
        // is read "\r\n", which is read "\n"): stable once they are gone
        std::string text = first;
        bool stable = false;
        for (int round = 0; round < 64 && !stable; ++round) {
            csv::reader again(string(text), plain);
            Records d = records(again);
            check(!d.failed && d.rows.size() == a.rows.size());
            std::string next = written(d, plain);
            stable = next == text;
            text = next;
        }
        check(stable);
    }

    void varints(std::string_view in, size_t piece) {
        slice<const byte> bytes(reinterpret_cast<const byte*>(in.data()), in.size());
        auto r = varint::read(bytes);
        io::buffered_reader stream(make_tracked<dribble>(std::string(in), piece));
        auto s = varint::read(stream);
        if (r) {
            check(r->second >= 1 && r->second <= varint::max_size && r->second <= in.size());
            check(s.has_value() && s->has_value() && **s == r->first);
            vector<byte> shortest;
            varint::append(shortest, r->first);
            check(shortest.size() <= r->second);
            auto again = varint::read(shortest);
            check(again.has_value() && again->first == r->first && again->second == shortest.size());
            auto sr = varint::read_signed(bytes);
            check(sr.has_value());
            vector<byte> signed_bytes;
            varint::append_signed(signed_bytes, sr->first);
            auto sa = varint::read_signed(signed_bytes);
            check(sa.has_value() && sa->first == sr->first);
        } else {
            check(r.error().offset() <= in.size());
            check(!s.has_value() || !s->has_value());
        }
        // the fixed widths, both orders, as far as the bytes go
        if (in.size() >= 8) {
            uint64_t v = big_endian::read_u64(bytes);
            vector<byte> out;
            big_endian::append_u64(out, v);
            check(view(out) == in.substr(0, 8));
            vector<byte> le;
            little_endian::append_u64(le, little_endian::read_u64(bytes));
            check(view(le) == in.substr(0, 8));
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2 || size > 65536) {
        return 0;
    }
    uint8_t mode = data[0];
    size_t piece = 1 + data[size - 1] % 17;
    std::string_view in(reinterpret_cast<const char*>(data + 1), size - 2);
    uint8_t options = mode >> 4;
    switch (mode % 16) {
        case 0: codec(base64::standard, true, false, in, piece); break;
        case 1: codec(base64::url, true, false, in, piece); break;
        case 2: codec(base64::raw_standard, true, false, in, piece); break;
        case 3: codec(base64::raw_url, true, false, in, piece); break;
        case 4: codec(base64::standard.lenient(), false, false, in, piece); break;
        case 5: codec(base32::standard, true, false, in, piece); break;
        case 6: codec(base32::hex.without_padding(), true, false, in, piece); break;
        case 7: codec(base32::standard.lenient(), false, false, in, piece); break;
        case 8: codec(ascii85(), false, false, in, piece); break;
        case 9: codec(hex(), true, true, in, piece); break;
        case 10:
        case 11: pem_blocks(in); break;
        case 12:
        case 13:
        case 14: csv_text(uint8_t(options | (mode & 3) << 4), in, piece); break;
        case 15: varints(in, piece); break;
    }
    return 0;
}
