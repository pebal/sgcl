//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "base64.h"
#include "error.h"
#include "detail/radix.h"
#include "../core/ordered_map.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sgcl::encoding {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        // A line of the text from at: its characters without the line
        // ending ([begin, end)) and where the next one starts
        struct PemLine {
            size_t begin;
            size_t end;
            size_t next;
        };

        inline PemLine pem_line(std::string_view v, size_t at) noexcept {
            size_t nl = v.find('\n', at);
            size_t end = nl == std::string_view::npos ? v.size() : nl;
            size_t next = nl == std::string_view::npos ? v.size() : nl + 1;
            if (end > at && v[end - 1] == '\r') {
                --end;
            }
            return {at, end, next};
        }

        // The white space RFC 7468 lets a parser skip in the base64 of a
        // block: space, tab, the line endings, vertical tab, form feed
        inline bool pem_space(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
        }

        inline std::string_view pem_trim(std::string_view s) noexcept {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
                s.remove_prefix(1);
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
                s.remove_suffix(1);
            }
            return s;
        }

        // A label of RFC 7468 section 3: printable ASCII but '-', words
        // joined by one '-' or one space, or nothing: "CERTIFICATE",
        // "X509 CRL", "RSA PRIVATE KEY"
        inline bool pem_label(std::string_view s) noexcept {
            bool after_separator = true;
            for (char ch : s) {
                auto c = uint8_t(ch);
                if ((c >= 0x21 && c <= 0x2C) || (c >= 0x2E && c <= 0x7E)) {
                    after_separator = false;
                } else if (c == '-' || c == ' ') {
                    if (after_separator) {
                        return false;
                    }
                    after_separator = true;
                } else {
                    return false;
                }
            }
            return s.empty() || !after_separator;
        }

        // The standard alphabet with its padding, strict: a key or a
        // certificate has one encoding
        inline constexpr Radix<6> PemBase64 {"base64", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", '='};
    }

    // A PEM block (RFC 7468): a type, the bytes, and the headers of RFC 1421
    // that older encrypted keys carry (Proc-Type, DEK-Info), written as
    //
    //     -----BEGIN CERTIFICATE-----
    //     MIIB...(base64, lines of 64)
    //     -----END CERTIFICATE-----
    //
    // A value that holds its bytes and headers in managed containers, so it
    // lives where a tracked_ptr may. parse takes the first block of a text
    // and parse_all every one of them; the text around and between the
    // blocks is skipped (section 5.2 lets a file explain itself there).
    // A block that is there and malformed — a BEGIN with no END, an END of
    // another type, base64 that is not — is an error with its line and
    // column, not a block passed over, as Go's pem.Decode passes over it.
    class pem {
    public:
        using error = encoding::error;

        // The type must be a label of the RFC ("CERTIFICATE", "EC PRIVATE
        // KEY"); a header's name must not be empty, hold ':', white space
        // or a control character, or start with "-----"; a value must not
        // hold a control character (a tab inside it is one it may) or
        // start or end with white space: anything else would not read back
        // as it was written, and is invalid_argument
        pem(const string& type, vector<byte> bytes)
        : _type(type), _bytes(std::move(bytes)) {
            _check();
        }

        pem(const string& type, vector<byte> bytes, ordered_map<string, string> headers)
        : _type(type), _bytes(std::move(bytes)), _headers(std::move(headers)) {
            _check();
        }

        const string& type() const noexcept {
            return _type;
        }

        const vector<byte>& bytes() const noexcept {
            return _bytes;
        }

        // In the order of the text; a name given twice keeps its last value
        const ordered_map<string, string>& headers() const noexcept {
            return _headers;
        }

        // The block as section 3's strict grammar writes it: lines of 64
        // characters, "\n" at every end, Proc-Type first among the headers
        // (RFC 1421 wants it there) and the rest in their order, then an
        // empty line
        string to_string() const {
            std::string s = "-----BEGIN ";
            s.append(_type.data(), _type.size());
            s += "-----\n";
            auto header = [&](const string& k, const string& v) {
                s.append(k.data(), k.size());
                s += ": ";
                s.append(v.data(), v.size());
                s += '\n';
            };
            auto proc = _headers.find(string("Proc-Type"));
            if (proc != _headers.end()) {
                header(proc->first, proc->second);
            }
            for (auto& [k, v] : _headers) {
                if (k != "Proc-Type") {
                    header(k, v);
                }
            }
            if (!_headers.empty()) {
                s += '\n';
            }
            auto text = base64::standard.encode(_bytes.as_slice());
            auto t = text.view();
            for (size_t i = 0; i < t.size(); i += 64) {
                s.append(t.substr(i, 64));
                s += '\n';
            }
            s += "-----END ";
            s.append(_type.data(), _type.size());
            s += "-----\n";
            return string(s);
        }

        // The first block of the text; a text with none is unexpected_end
        static expected<pem, error> parse(const string& text) {
            size_t next = 0;
            auto r = _parse(text, 0, next);
            if (!r) {
                return unexpected<error>(std::move(r.error()));
            }
            if (!*r) {
                return unexpected<error>(error(errc::unexpected_end, text.size(), string("no PEM block")).locate(text));
            }
            return std::move(**r);
        }

        // Every block of the text, none for a text with none
        static expected<vector<pem>, error> parse_all(const string& text) {
            vector<pem> out;
            size_t at = 0;
            for (;;) {
                size_t next = 0;
                auto r = _parse(text, at, next);
                if (!r) {
                    return unexpected<error>(std::move(r.error()));
                }
                if (!*r) {
                    return out;
                }
                out.push_back(std::move(**r));
                at = next;
            }
        }

    private:
        pem() = default;

        void _check() const {
            if (!detail::pem_label(_type.view())) {
                throw invalid_argument("sgcl::pem: a type that is not a label of RFC 7468");
            }
            for (auto& [k, v] : _headers) {
                if (k.empty()) {
                    throw invalid_argument("sgcl::pem: a header without a name");
                }
                if (k.view().starts_with("-----")) {
                    throw invalid_argument("sgcl::pem: a header name that reads as a boundary line");
                }
                for (char c : k.view()) {
                    if (c == ':' || uint8_t(c) <= ' ' || uint8_t(c) == 0x7F) {
                        throw invalid_argument("sgcl::pem: a header name with ':', white space or a control character");
                    }
                }
                auto value = v.view();
                if (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.back() == ' ' || value.back() == '\t')) {
                    throw invalid_argument("sgcl::pem: a header value with white space at an end, which a reader trims");
                }
                for (char c : value) {
                    if ((uint8_t(c) < ' ' && c != '\t') || uint8_t(c) == 0x7F) {
                        throw invalid_argument("sgcl::pem: a header value with a control character");
                    }
                }
            }
        }

        static error _fail(const string& text, errc code, size_t at, const std::string& what) {
            return error(code, at, string(what)).locate(text);
        }

        // The boundary line at `at`, "-----" + word + label + "-----" and
        // white space: the label, or the error
        static expected<std::string_view, error> _boundary(const string& text, size_t at, size_t prefix, const char* word) {
            auto v = text.view();
            auto line = detail::pem_line(v, at);
            size_t from = at + prefix;
            size_t close = v.find("-----", from);
            if (close == std::string_view::npos || close + 5 > line.end) {
                return unexpected<error>(_fail(text, errc::syntax, line.end, std::string("a ") + word + " line without its closing dashes"));
            }
            auto label = v.substr(from, close - from);
            if (!detail::pem_label(label)) {
                return unexpected<error>(_fail(text, errc::syntax, from, "a type that is not a label of RFC 7468"));
            }
            for (size_t i = close + 5; i < line.end; ++i) {
                if (v[i] != ' ' && v[i] != '\t') {
                    return unexpected<error>(_fail(text, errc::syntax, i, std::string("text after the dashes of the ") + word + " line"));
                }
            }
            return label;
        }

        // Whether the header lines from at end in an empty line: every
        // line before it has a colon or starts with white space
        static bool _headers_end_empty(std::string_view v, size_t at) noexcept {
            while (at < v.size()) {
                auto line = detail::pem_line(v, at);
                auto content = v.substr(line.begin, line.end - line.begin);
                if (detail::pem_trim(content).empty()) {
                    return true;
                }
                bool indented = content[0] == ' ' || content[0] == '\t';
                if (content.starts_with("-----") || (!indented && content.find(':') == std::string_view::npos)) {
                    return false;
                }
                at = line.next;
            }
            return false;
        }

        // The first boundary line of the kind at or after from: its start,
        // or npos
        static size_t _find_line(std::string_view v, size_t from, std::string_view start) noexcept {
            for (size_t at = from;;) {
                at = v.find(start, at);
                if (at == std::string_view::npos || at == 0 || v[at - 1] == '\n') {
                    return at;
                }
                ++at;
            }
        }

        // The block that starts first at or after from, and where the text
        // after it starts; nullopt when there is none
        static expected<optional<pem>, error> _parse(const string& text, size_t from, size_t& next) {
            auto v = text.view();
            size_t at = _find_line(v, from, "-----BEGIN ");
            if (at == std::string_view::npos) {
                return optional<pem>();
            }
            auto label = _boundary(text, at, 11, "BEGIN");
            if (!label) {
                return unexpected<error>(std::move(label.error()));
            }
            at = detail::pem_line(v, at).next;

            // The headers of RFC 1421, when the first line has a colon: a
            // name and a value a line. RFC 1421 ends them with an empty
            // line, and only then may a line that starts with white space
            // go on with the value before it (the folding of RFC 822): a
            // block whose headers end in an empty line is read so. One
            // with no empty line after them is read as Go reads it — the
            // headers are the lines with a colon, and the first line
            // without one, indented or not, is the start of the base64 —
            // so that an indented line of base64 is never taken into a
            // header's value.
            std::vector<std::pair<std::string, std::string>> headers;
            if (at < v.size()) {
                auto first = detail::pem_line(v, at);
                if (v.substr(first.begin, first.end - first.begin).find(':') != std::string_view::npos) {
                    bool folded = _headers_end_empty(v, at);
                    while (at < v.size()) {
                        auto line = detail::pem_line(v, at);
                        auto content = v.substr(line.begin, line.end - line.begin);
                        if (detail::pem_trim(content).empty()) {
                            if (folded) {
                                at = line.next;
                            }
                            break;
                        }
                        if (folded && (content[0] == ' ' || content[0] == '\t')) {
                            if (headers.empty()) {
                                return unexpected<error>(_fail(text, errc::syntax, line.begin, "a header line going on from no header"));
                            }
                            headers.back().second += content;
                            at = line.next;
                            continue;
                        }
                        size_t colon = content.find(':');
                        if (colon == std::string_view::npos || content.starts_with("-----")) {
                            break;
                        }
                        auto key = detail::pem_trim(content.substr(0, colon));
                        if (key.empty()) {
                            return unexpected<error>(_fail(text, errc::syntax, line.begin, "a header without a name"));
                        }
                        headers.emplace_back(std::string(key), std::string(detail::pem_trim(content.substr(colon + 1))));
                        at = line.next;
                    }
                }
            }

            size_t end = _find_line(v, at, "-----END ");
            if (end == std::string_view::npos) {
                return unexpected<error>(_fail(text, errc::unexpected_end, v.size(), "no END line for BEGIN " + std::string(*label)));
            }

            // The base64 between, white space skipped anywhere: each run of
            // characters fed at its own offset, so that an error names its
            // place in the whole text
            const auto& codec = detail::PemBase64;
            vector<byte> bytes;
            bytes.resize(codec.max_decoded_size(end - at));
            detail::Radix<6>::decoding d;
            optional<error> e;
            auto out = reinterpret_cast<uint8_t*>(bytes.data());
            auto out_end = out + bytes.size();
            for (size_t i = at; i < end;) {
                if (detail::pem_space(v[i])) {
                    ++i;
                    continue;
                }
                size_t j = i;
                while (j < end && !detail::pem_space(v[j])) {
                    ++j;
                }
                d.offset = i;
                auto p = v.data() + i;
                auto status = codec.feed(d, p, v.data() + j, out, out_end, e);
                if (status == detail::FeedStatus::failed) {
                    return unexpected<error>(e->locate(text));
                }
                if (status == detail::FeedStatus::room) {
                    throw logic_error("sgcl::pem: a decoding ran out of the room its bound gave");
                }
                i = j;
            }
            d.offset = end;
            auto status = codec.finish(d, out, out_end, e);
            if (status == detail::FeedStatus::failed) {
                return unexpected<error>(e->locate(text));
            }
            if (status == detail::FeedStatus::room) {
                throw logic_error("sgcl::pem: a decoding ran out of the room its bound gave");
            }
            bytes.resize(size_t(out - reinterpret_cast<uint8_t*>(bytes.data())));

            auto end_label = _boundary(text, end, 9, "END");
            if (!end_label) {
                return unexpected<error>(std::move(end_label.error()));
            }
            if (*end_label != *label) {
                return unexpected<error>(_fail(text, errc::syntax, end + 9, "END " + std::string(*end_label) + " does not match BEGIN " + std::string(*label)));
            }
            next = detail::pem_line(v, end).next;

            pem block;
            block._type = string(*label);
            block._bytes = std::move(bytes);
            for (auto& [k, value] : headers) {
                block._headers.insert_or_assign(string(k), string(detail::pem_trim(value)));
            }
            return optional<pem>(std::move(block));
        }

        string _type;
        vector<byte> _bytes;
        ordered_map<string, string> _headers;
    };
}
