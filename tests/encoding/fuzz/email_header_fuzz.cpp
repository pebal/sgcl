//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The pieces of a mail's head on any text: address lists, dates, encoded
// words, the parameters of RFC 2045 and RFC 2231, and the quoted-printable
// codec. The first byte picks the piece. What must hold:
//   - an address list that parses is written, address by address, to a
//     text that parses to the same names and addr-specs; one address alone
//     parses as itself;
//   - unstructured text written as a field (encoded words where needed)
//     reads back as the same text, its line breaks spaces;
//   - a parameter written by the library reads back as its value;
//   - quoted-printable decoded strictly fails or gives what the lenient
//     decoding gives; decoded in pieces of a size the input picks, it gives
//     what the whole text gives; any bytes encoded (text and binary) decode
//     back to themselves (binary) or to themselves with their line breaks
//     CRLF (text);
//   - a date and a part's head are read without a crash.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/email_header_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/email.h"

#include <cstdint>
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

    std::string str(const string& s) {
        return std::string(s.view());
    }

    std::string str(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    void addresses(std::string_view in) {
        auto list = email::address::parse_list(string(in));
        if (!list) {
            return;
        }
        std::string text;
        for (auto& a : *list) {
            if (!text.empty()) {
                text += ", ";
            }
            text += str(a.to_string());
            // a name or an addr-spec with a control can be written but is not one to read back
        }
        bool clean = true;
        for (auto& a : *list) {
            for (char c : std::string_view(a.addr().view())) {
                clean &= uint8_t(c) >= 0x20 && uint8_t(c) < 0x7F;   // a domain past ASCII is written by IDNA
            }
            for (char c : std::string_view(a.name().view())) {
                clean &= uint8_t(c) >= 0x20 && c != 0x7F;
            }
            clean &= !std::string_view(a.name().view()).starts_with(' ') && !std::string_view(a.name().view()).ends_with(' ');
            clean &= std::string_view(a.name().view()).find("  ") == std::string_view::npos;
        }
        if (!clean) {
            return;
        }
        auto again = email::address::parse_list(string(text));
        check(again.has_value());
        check(again->size() == list->size());
        for (size_t i = 0; i < list->size(); ++i) {
            check((*again)[i].addr() == (*list)[i].addr());
            check((*again)[i].name() == (*list)[i].name());
            auto one = email::address::parse((*list)[i].to_string());
            check(one.has_value() && one->addr() == (*list)[i].addr());
        }
    }

    void unstructured(std::string_view in) {
        email m;
        m.set_subject(string(in));
        auto out = m.to_string();
        auto back = email::parse(out);
        check(back.has_value());
        std::string want;
        for (char c : in) {
            want += (c == '\r' || c == '\n') ? ' ' : c;
        }
        // the reader trims the value and unfolds it
        std::string_view w = want;
        while (!w.empty() && (w.front() == ' ' || w.front() == '\t')) {
            w.remove_prefix(1);
        }
        while (!w.empty() && (w.back() == ' ' || w.back() == '\t')) {
            w.remove_suffix(1);
        }
        bool printable = true;
        for (char c : w) {
            printable &= uint8_t(c) >= 0x20 || c == '\t';
        }
        if (printable && w.find("=?") == std::string_view::npos) {
            check(str(back->subject()) == w);
        }
    }

    void params(std::string_view in) {
        // a value written as a file's name reads back as itself
        email::part p("application/octet-stream", vector<byte>());
        std::string name(in);
        for (char& c : name) {
            if (uint8_t(c) < 0x20 || c == 0x7F) {
                c = '_';
            }
        }
        p.set_filename(string(name));
        email m;
        m.set_body(p);
        auto back = email::parse(m.to_string());
        check(back.has_value());
        check(str(back->body().filename()) == name);
        // any text as a part's head
        auto raw = email::parse(string(std::string("Content-Type: ") + std::string(in) + "\r\nContent-Disposition: " + std::string(in) +
                                       "\r\n\r\nx"));
        if (raw) {
            (void)raw->body().filename();
            (void)raw->body().param("charset");
            (void)raw->body().text();
        }
    }

    void qp(std::string_view in, size_t piece) {
        auto lax = quoted_printable::standard.lenient().decode(string(in));
        check(lax.has_value());
        auto strict = quoted_printable::standard.decode(string(in));
        if (strict) {
            check(str(*strict) == str(*lax));
        }
        // in pieces
        tracked_ptr src = make_tracked<io::buffer>(string(in));
        auto dec = quoted_printable::standard.lenient().decoder_from(io::reader(src));
        std::string streamed;
        vector<byte> buf(piece);
        for (;;) {
            auto n = dec.read(buf.as_slice());
            check(n.has_value());
            if (*n == 0) {
                break;
            }
            streamed.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
        check(streamed == str(*lax));
        // encoded back
        vector<byte> bytes;
        for (char c : in) {
            bytes.push_back(byte(uint8_t(c)));
        }
        auto bin = quoted_printable::standard.binary().encode(bytes);
        auto back = quoted_printable::standard.decode(bin);
        check(back.has_value() && str(*back) == std::string(in));
        auto text = quoted_printable::standard.encode(bytes);
        auto tback = quoted_printable::standard.decode(text);
        check(tback.has_value());
        std::string crlf;
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '\n') {
                crlf += "\r\n";
            } else if (in[i] == '\r' && i + 1 < in.size() && in[i + 1] == '\n') {
                crlf += "\r\n";
                ++i;
            } else {
                crlf += in[i];
            }
        }
        check(str(*tback) == crlf);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    const uint8_t mode = data[0];
    const size_t piece = size_t(data[1] % 64) + 1;
    std::string_view in(reinterpret_cast<const char*>(data + 2), size - 2);
    switch (mode % 6) {
        case 0:
            addresses(in);
            break;
        case 1:
            unstructured(in);
            break;
        case 2:
            params(in);
            break;
        case 3:
            qp(in, piece);
            break;
        case 4: {
            auto m = email::parse(string(std::string("Date: ") + std::string(in) + "\r\n\r\n"));
            if (m) {
                (void)m->date();
            }
            break;
        }
        case 5: {
            auto m = email::parse(string(std::string("Subject: ") + std::string(in) + "\r\nTo: " + std::string(in) + "\r\n\r\n"));
            if (m) {
                (void)m->subject();
                (void)m->to();
                (void)m->headers();
            }
            break;
        }
    }
    return 0;
}
