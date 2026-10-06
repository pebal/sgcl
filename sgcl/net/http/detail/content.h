//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../headers.h"
#include "../../../core/aliases.h"

#include <cstdint>
#include <string_view>
#include <vector>

// What a server answers about a representation's validators and ranges,
// pure functions over the request's fields: the Range field read (RFC 9110
// §14.1.2), the lists of entity tags of If-Match and If-None-Match (§8.8.3,
// §13.1.1, §13.1.2), the order of the preconditions (§13.2.2) and If-Range
// (§13.1.5). serve.h answers with them; the client's cache (cache.h) and a
// download's resumption read the same forms from the other side.
namespace sgcl::net::http::detail {
    // A range of a representation: its first byte and its length (never 0)
    struct ByteRange {
        uint64_t start = 0;
        uint64_t length = 0;
    };

    // What a Range field asks of a representation of `size` bytes
    enum class RangeVerdict : uint8_t {
        whole,           // no range to answer (another unit, no ranges, overlaps past the size, too many): 200
        ranges,          // the ranges, satisfiable, in their order: 206
        unsatisfiable,   // every range starts past the end: 416 with Content-Range: bytes */size
        invalid,         // a range that is not one (letters, last before first, a number past 2^63): 416
    };

    struct RangeSet {
        RangeVerdict verdict = RangeVerdict::whole;
        std::vector<ByteRange> ranges;   // scratch of the request: plain memory, no tracked word
    };

    // More ranges than this in one field are answered with the whole body
    // (Apache's MaxRanges is 200; a hundred covers what clients ask)
    inline constexpr size_t MaxRanges = 100;

    // A decimal number of 1*DIGIT within 2^63 - 1; false for anything else
    inline bool range_number(std::string_view s, uint64_t& out) noexcept {
        if (s.empty()) {
            return false;
        }
        size_t zeros = 0;
        while (zeros + 1 < s.size() && s[zeros] == '0') {
            ++zeros;
        }
        s.remove_prefix(zeros);
        if (s.size() > 19) {
            return false;
        }
        uint64_t v = 0;
        for (char c : s) {
            if (c < '0' || c > '9') {
                return false;
            }
            v = v * 10 + uint64_t(c - '0');
        }
        if (v > uint64_t(INT64_MAX)) {
            return false;
        }
        out = v;
        return true;
    }

    // The Range field of a request for a representation of `size` bytes,
    // as RFC 9110 §14.1.2 reads it and Go's ServeContent answers it: a unit
    // other than bytes ignored (the RFC's MUST; Go refuses it), the members
    // of the set split at commas with OWS about them and empty ones passed
    // over; `first-last` (last past the end cut to it), `first-` (to the
    // end), `-suffix` (the last suffix bytes, all of them when the
    // representation is shorter; `-0` unsatisfiable, RFC §14.1.3). A first
    // byte at or past the end is a range that does not overlap and is
    // dropped; none left is 416. The sum of the lengths past the size (an
    // overlap meant to multiply the work) or more than MaxRanges ranges is
    // answered whole, as Go does with the first
    inline RangeSet parse_ranges(std::string_view field, uint64_t size) noexcept {
        RangeSet out;
        field = trim_ows(field);
        const size_t eq = field.find('=');
        if (eq == std::string_view::npos || !iequal(field.substr(0, eq), "bytes")) {
            return out;   // no unit we know: the whole representation
        }
        std::string_view set = field.substr(eq + 1);
        bool no_overlap = false;
        uint64_t total = 0;
        while (!set.empty()) {
            const size_t comma = set.find(',');
            std::string_view member = trim_ows(set.substr(0, comma));
            set = comma == std::string_view::npos ? std::string_view() : set.substr(comma + 1);
            if (member.empty()) {
                continue;
            }
            const size_t dash = member.find('-');
            if (dash == std::string_view::npos) {
                out.verdict = RangeVerdict::invalid;
                out.ranges.clear();
                return out;
            }
            const std::string_view first = trim_ows(member.substr(0, dash));
            const std::string_view last = trim_ows(member.substr(dash + 1));
            ByteRange r;
            if (first.empty()) {
                uint64_t suffix = 0;
                if (!range_number(last, suffix)) {
                    out.verdict = RangeVerdict::invalid;
                    out.ranges.clear();
                    return out;
                }
                if (suffix == 0 || size == 0) {
                    no_overlap = true;   // nothing of a representation is its last 0 bytes
                    continue;
                }
                if (suffix > size) {
                    suffix = size;
                }
                r.start = size - suffix;
                r.length = suffix;
            } else {
                uint64_t start = 0;
                if (!range_number(first, start)) {
                    out.verdict = RangeVerdict::invalid;
                    out.ranges.clear();
                    return out;
                }
                if (start >= size) {
                    no_overlap = true;
                    continue;
                }
                r.start = start;
                if (last.empty()) {
                    r.length = size - start;
                } else {
                    uint64_t end = 0;
                    if (!range_number(last, end) || start > end) {
                        out.verdict = RangeVerdict::invalid;
                        out.ranges.clear();
                        return out;
                    }
                    if (end >= size) {
                        end = size - 1;
                    }
                    r.length = end - start + 1;
                }
            }
            if (out.ranges.size() == MaxRanges) {
                out.ranges.clear();
                return out;   // whole
            }
            total += r.length;
            out.ranges.push_back(r);
        }
        if (out.ranges.empty()) {
            out.verdict = no_overlap ? RangeVerdict::unsatisfiable : RangeVerdict::whole;
            return out;
        }
        if (total > size) {
            out.ranges.clear();
            return out;   // overlapping ranges asking for more than there is: the whole body
        }
        out.verdict = RangeVerdict::ranges;
        return out;
    }

    // etagc of RFC 9110 §8.8.3: "!" / %x23-7E / obs-text
    SGCL_INLINE_HOT constexpr bool etag_char(uint8_t c) noexcept {
        return c == 0x21 || (c >= 0x23 && c != 0x7F);
    }

    // One entity tag at the start of s (OWS before it skipped): its text
    // with W/ and the quotes, and what follows it; an empty tag when s does
    // not start with one
    inline std::string_view scan_etag(std::string_view s, std::string_view& rest) noexcept {
        s = trim_ows(s);
        size_t at = 0;
        if (s.size() >= 2 && s[0] == 'W' && s[1] == '/') {
            at = 2;
        }
        if (s.size() - at < 2 || s[at] != '"') {
            return {};
        }
        for (size_t i = at + 1; i < s.size(); ++i) {
            const uint8_t c = uint8_t(s[i]);
            if (c == '"') {
                rest = s.substr(i + 1);
                return s.substr(0, i + 1);
            }
            if (!etag_char(c)) {
                return {};
            }
        }
        return {};
    }

    SGCL_INLINE_HOT constexpr bool etag_weak(std::string_view tag) noexcept {
        return tag.size() >= 2 && tag[0] == 'W' && tag[1] == '/';
    }

    SGCL_INLINE_HOT constexpr std::string_view etag_opaque(std::string_view tag) noexcept {
        return etag_weak(tag) ? tag.substr(2) : tag;
    }

    // An entity tag fit to be compared: exactly one tag, nothing after it
    inline bool valid_etag(std::string_view tag) noexcept {
        std::string_view rest;
        auto t = scan_etag(tag, rest);
        return !t.empty() && trim_ows(rest).empty() && t.size() == trim_ows(tag).size();
    }

    // The comparisons of RFC 9110 §8.8.3.2: strong, both tags not weak and
    // their opaque parts equal; weak, the opaque parts equal
    SGCL_INLINE_HOT bool etag_strong_match(std::string_view a, std::string_view b) noexcept {
        return !a.empty() && !etag_weak(a) && !etag_weak(b) && a == b;
    }

    SGCL_INLINE_HOT bool etag_weak_match(std::string_view a, std::string_view b) noexcept {
        return !a.empty() && !b.empty() && etag_opaque(a) == etag_opaque(b);
    }

    // Whether a list of If-Match or If-None-Match ("*", or tags separated by
    // commas) holds a tag that matches `ours` by the comparison asked; `*`
    // matches when there is a representation, which there is whenever this
    // is asked. A member that is not a tag ends the list there, as Go's
    // scan does: nothing after it matches
    inline bool etag_list_matches(std::string_view list, std::string_view ours, bool strong) noexcept {
        std::string_view s = list;
        for (;;) {
            s = trim_ows(s);
            if (s.empty()) {
                return false;
            }
            if (s[0] == ',') {
                s.remove_prefix(1);
                continue;
            }
            if (s[0] == '*') {
                return true;
            }
            std::string_view rest;
            auto tag = scan_etag(s, rest);
            if (tag.empty()) {
                return false;
            }
            if (strong ? etag_strong_match(tag, ours) : etag_weak_match(tag, ours)) {
                return true;
            }
            s = rest;
        }
    }

    // What the preconditions of a request decide (RFC 9110 §13.2.2)
    enum class Precondition : uint8_t {
        proceed,        // answer as asked (Range and If-Range next)
        not_modified,   // 304
        failed,         // 412
    };

    // The preconditions in the RFC's order, as Go's checkPreconditions
    // takes them: If-Match (strong; else If-Unmodified-Since), then
    // If-None-Match (weak: 304 for GET and HEAD, 412 for another method;
    // else, GET and HEAD alone, If-Modified-Since). `etag` is the
    // representation's tag ("" for none), `modified` its Last-Modified in
    // whole seconds (nullopt for none); a date that does not parse is no
    // condition
    inline Precondition check_preconditions(const http::headers& h, std::string_view method, std::string_view etag,
                                            optional<int64_t> modified) noexcept {
        const bool get_or_head = method == "GET" || method == "HEAD";
        if (auto im = HeadersAccess::find(h, "if-match")) {
            if (!etag_list_matches(*im, etag, true)) {
                return Precondition::failed;
            }
        } else if (modified && HeadersAccess::find(h, "if-unmodified-since")) {
            if (auto since = h.date("If-Unmodified-Since"); since && *modified > since->unix()) {
                return Precondition::failed;
            }
        }
        if (auto inm = HeadersAccess::find(h, "if-none-match")) {
            if (etag_list_matches(*inm, etag, false)) {
                return get_or_head ? Precondition::not_modified : Precondition::failed;
            }
        } else if (get_or_head && modified && HeadersAccess::find(h, "if-modified-since")) {
            if (auto since = h.date("If-Modified-Since"); since && *modified <= since->unix()) {
                return Precondition::not_modified;
            }
        }
        return Precondition::proceed;
    }

    // Whether the Range of a request is to be answered by its If-Range
    // (RFC 9110 §13.1.5): none, yes; an entity tag, by the strong
    // comparison; a date, when it is exactly the Last-Modified (Go's rule)
    inline bool if_range_allows(const http::headers& h, std::string_view etag, optional<int64_t> modified) noexcept {
        auto ir = HeadersAccess::find(h, "if-range");
        if (!ir) {
            return true;
        }
        std::string_view v = trim_ows(*ir);
        if (!v.empty() && (v[0] == '"' || (v.size() > 1 && v[0] == 'W' && v[1] == '/'))) {
            std::string_view rest;
            auto tag = scan_etag(v, rest);
            return !tag.empty() && etag_strong_match(tag, etag);
        }
        if (!modified) {
            return false;
        }
        auto t = h.date("If-Range");
        return t && t->unix() == *modified;
    }

    // "bytes first-last/size" of a Content-Range, or "bytes */size"
    inline std::string content_range(const ByteRange* r, uint64_t size) {
        std::string out = "bytes ";
        if (r) {
            out += std::to_string(r->start);
            out += '-';
            out += std::to_string(r->start + r->length - 1);
        } else {
            out += '*';
        }
        out += '/';
        out += std::to_string(size);
        return out;
    }

    // A Content-Range of a response read back (a download's resumption):
    // the first byte, the last and the complete length (nullopt for "*");
    // "bytes */size" as first = last = UINT64_MAX
    struct ContentRange {
        uint64_t first = 0;
        uint64_t last = 0;
        optional<uint64_t> size;
        bool unsatisfied = false;
    };

    inline optional<ContentRange> parse_content_range(std::string_view v) noexcept {
        v = trim_ows(v);
        if (v.size() < 6 || !iequal(v.substr(0, 6), "bytes ")) {
            return nullopt;
        }
        v = trim_ows(v.substr(6));
        const size_t slash = v.find('/');
        if (slash == std::string_view::npos) {
            return nullopt;
        }
        ContentRange out;
        std::string_view range = v.substr(0, slash);
        std::string_view total = v.substr(slash + 1);
        if (total != "*") {
            uint64_t n = 0;
            if (!range_number(total, n)) {
                return nullopt;
            }
            out.size = n;
        }
        if (range == "*") {
            if (!out.size) {
                return nullopt;
            }
            out.unsatisfied = true;
            return out;
        }
        const size_t dash = range.find('-');
        if (dash == std::string_view::npos || !range_number(range.substr(0, dash), out.first) ||
            !range_number(range.substr(dash + 1), out.last) || out.first > out.last || (out.size && out.last >= *out.size)) {
            return nullopt;
        }
        return out;
    }
}
