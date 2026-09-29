//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// slog's values on any bytes, written and read back (DESIGN 283, the
// auditor's condition 5). The first byte picks the path; a key and a text
// are split at a NUL. What must hold:
//   - text: a line of the text handler reads back, by a reader of
//     key=value pairs written here (Go's quoting: a text in quotes when it
//     has to be, strconv's escapes inside), to the message, the key (with
//     its group's name and a dot before it) and the value exactly as they
//     were given, whatever bytes they hold — invalid UTF-8 included,
//     which the escapes keep byte for byte;
//   - JSON: a line of the JSON handler is JSON that json::parse reads,
//     with the message, the key and the value as given, every invalid
//     byte of UTF-8 in them one U+FFFD; a vector of the texts, a
//     container of a described type written as json.Marshal does, too;
//   - numbers: a float reads back to its bits from both lines (NaN as
//     NaN; a NaN or an infinity in JSON as slog's error string), an
//     integer to itself, a duration's text through duration::parse.
// Built with libFuzzer (tests/fuzz/run.sh tests/slog/fuzz/slog_values_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/slog/slog.h"
#include "sgcl/encoding/json.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    std::string text_of(const io::buffer& b) {
        auto d = b.data();
        return std::string(reinterpret_cast<const char*>(d.data()), d.size());
    }

    int hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    void put_utf8(std::string& out, uint32_t c) {
        if (c < 0x80) {
            out += char(c);
        } else if (c < 0x800) {
            out += char(0xC0 | (c >> 6));
            out += char(0x80 | (c & 63));
        } else if (c < 0x10000) {
            out += char(0xE0 | (c >> 12));
            out += char(0x80 | ((c >> 6) & 63));
            out += char(0x80 | (c & 63));
        } else {
            out += char(0xF0 | (c >> 18));
            out += char(0x80 | ((c >> 12) & 63));
            out += char(0x80 | ((c >> 6) & 63));
            out += char(0x80 | (c & 63));
        }
    }

    // One token of a text line at `at`: a text in quotes (strconv.Unquote)
    // or a bare run to `stop`; false when the line is not well formed
    bool token(std::string_view line, size_t& at, char stop, std::string& out) {
        out.clear();
        if (at < line.size() && line[at] == '"') {
            ++at;
            while (at < line.size() && line[at] != '"') {
                char c = line[at++];
                if (c != '\\') {
                    check(!((unsigned char)c < 0x20 || c == 0x7f));   // no raw control inside quotes
                    out += c;
                    continue;
                }
                if (at >= line.size()) {
                    return false;
                }
                char e = line[at++];
                switch (e) {
                    case 'a': out += '\a'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'v': out += '\v'; break;
                    case '\\': out += '\\'; break;
                    case '"': out += '"'; break;
                    case 'x': {
                        if (at + 2 > line.size() || hex(line[at]) < 0 || hex(line[at + 1]) < 0) {
                            return false;
                        }
                        out += char(hex(line[at]) * 16 + hex(line[at + 1]));
                        at += 2;
                        break;
                    }
                    case 'u':
                    case 'U': {
                        size_t n = e == 'u' ? 4 : 8;
                        if (at + n > line.size()) {
                            return false;
                        }
                        uint32_t c = 0;
                        for (size_t i = 0; i < n; ++i) {
                            int h = hex(line[at + i]);
                            if (h < 0) {
                                return false;
                            }
                            c = c * 16 + uint32_t(h);
                        }
                        at += n;
                        put_utf8(out, c);
                        break;
                    }
                    default:
                        return false;
                }
            }
            if (at >= line.size()) {
                return false;
            }
            ++at;
            return true;
        }
        size_t start = at;
        while (at < line.size() && line[at] != stop && line[at] != '\n') {
            ++at;
        }
        out.assign(line.substr(start, at - start));
        return true;
    }

    // The bytes as JSON has them: every byte of invalid UTF-8 one U+FFFD
    std::string sanitized(std::string_view s) {
        std::string out;
        for (size_t i = 0; i < s.size();) {
            auto [c, n] = utf8::decode(s, i);
            if (c == utf8::replacement && n == 1) {
                out += "\xEF\xBF\xBD";
                i += 1;
            } else {
                out.append(s.substr(i, n));
                i += n;
            }
        }
        return out;
    }

    std::string as_std(const string& s) {
        return std::string(s.data(), s.size());
    }

    // The text handler: msg, and key=value with the group's name before
    // the key when there is one
    void text_round_trip(std::string_view group, std::string_view key, std::string_view value) {
        io::buffer out;
        auto lg = slog::logger(slog::options{.out = out, .utc = true});
        std::string k(key);
        std::string g(group);
        slice<const char> v(value.data(), value.size());
        if (g.empty()) {
            lg.info(v, k.c_str(), v);
        } else {
            lg.group(g.c_str()).info(v, k.c_str(), v);
        }
        std::string line = text_of(out);
        check(!line.empty() && line.back() == '\n');
        check(line.find('\n') == line.size() - 1);   // one line
        size_t at = line.find(" level=INFO msg=");
        check(at != std::string::npos);
        at += 16;
        std::string msg, key_back, value_back;
        check(token(line, at, ' ', msg));
        check(msg == std::string(value));
        check(at < line.size() && line[at] == ' ');
        ++at;
        check(token(line, at, '=', key_back));
        check(at < line.size() && line[at] == '=');
        ++at;
        check(token(line, at, ' ', value_back));
        check(at == line.size() - 1);
        check(key_back == (g.empty() ? std::string(key) : g + "." + std::string(key)));
        check(value_back == std::string(value));
    }

    struct Texts {
        vector<string> v;

        void describe(encoding::field_list& f) {
            f.add("v", v);
        }
    };

    void json_round_trip(std::string_view key, std::string_view value) {
        io::buffer out;
        auto lg = slog::logger(slog::options{.out = out, .json = true, .utc = true}).group("g");
        std::string k(key);
        slice<const char> v(value.data(), value.size());
        Texts texts;
        texts.v.push_back(string(value.data(), value.size()));
        texts.v.push_back(string(key.data(), key.size()));
        lg.info(v, k.c_str(), v);
        std::string line = text_of(out);
        check(!line.empty() && line.back() == '\n');
        check(line.find('\n') == line.size() - 1);   // one line
        auto j = encoding::json::parse(string(line.data(), line.size() - 1));
        check(j.has_value());
        check(as_std(*(*j)["msg"].as_string()) == sanitized(value));
        check(as_std(*(*j)["g"][string(sanitized(key))].as_string()) == sanitized(value));
        // the same texts inside a value of the program, as json.Marshal
        out.clear();
        lg.info("m", "texts", texts);
        line = text_of(out);
        j = encoding::json::parse(string(line.data(), line.size() - 1));
        check(j.has_value());
        const auto& list = (*j)["g"]["texts"]["v"];
        check(list.size() == 2);
        check(as_std(*list[0].as_string()) == sanitized(value));
        check(as_std(*list[1].as_string()) == sanitized(key));
    }

    void float_round_trip(double f) {
        io::buffer out;
        slog::logger(slog::options{.out = out, .utc = true}).info("m", "f", f);
        std::string line = text_of(out);
        size_t at = line.find(" f=");
        check(at != std::string::npos);
        std::string t = line.substr(at + 3, line.size() - at - 4);
        if (std::isnan(f)) {
            check(t == "NaN");
        } else if (std::isinf(f)) {
            check(t == (f < 0 ? "-Inf" : "+Inf"));
        } else {
            double back = std::strtod(t.c_str(), nullptr);
            check(std::memcmp(&back, &f, sizeof f) == 0);
        }
        out.clear();
        slog::logger(slog::options{.out = out, .json = true, .utc = true}).info("m", "f", f);
        line = text_of(out);
        auto j = encoding::json::parse(string(line.data(), line.size() - 1));
        check(j.has_value());
        const auto& v = (*j)["f"];
        if (!std::isfinite(f)) {
            check(v.as_string().has_value());
            check(as_std(*v.as_string()).rfind("!ERROR:json: unsupported value: ", 0) == 0);
        } else {
            auto d = v.as_double();
            check(d.has_value());
            double back = *d;
            check(back == f && std::signbit(back) == std::signbit(f));
        }
    }

    void integer_round_trip(int64_t i) {
        io::buffer out;
        const sgcl::duration d{std::chrono::nanoseconds(i)};
        slog::logger(slog::options{.out = out, .utc = true}).info("m", "i", i, "d", d);
        std::string line = text_of(out);
        size_t a = line.find(" i=");
        size_t b = line.find(" d=");
        check(a != std::string::npos && b != std::string::npos);
        check(std::strtoll(line.c_str() + a + 3, nullptr, 10) == i);
        std::string dt = line.substr(b + 3, line.size() - b - 4);
        auto back = sgcl::duration::parse(string(dt));
        check(back.has_value());
        check(back->nanoseconds() == i);
        out.clear();
        slog::logger(slog::options{.out = out, .json = true, .utc = true}).info("m", "i", i, "d", d);
        line = text_of(out);
        auto j = encoding::json::parse(string(line.data(), line.size() - 1));
        check(j.has_value());
        check((*j)["i"].as_int() == i);
        check((*j)["d"].as_int() == i);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    const uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    size_t nul = rest.find('\0');
    std::string_view key = rest.substr(0, nul);
    std::string_view value = nul == std::string_view::npos ? std::string_view() : rest.substr(nul + 1);
    switch (mode % 5) {
        case 0:
            text_round_trip({}, key, value);
            break;
        case 1: {
            // the group's name: the key's first part, to its first space
            // or '.' (a dot inside a name would make the split ambiguous)
            size_t cut = key.find_first_of(" .");
            std::string_view g = key.substr(0, cut);
            text_round_trip(g, key, value);
            break;
        }
        case 2:
            json_round_trip(key, value);
            break;
        case 3: {
            uint64_t bits = 0;
            sgcl::detail::copy_bytes(&bits, rest.data(), rest.size() < 8 ? rest.size() : 8);
            const double f = std::bit_cast<double>(bits);
            float_round_trip(f);
            break;
        }
        default: {
            uint64_t bits = 0;
            sgcl::detail::copy_bytes(&bits, rest.data(), rest.size() < 8 ? rest.size() : 8);
            integer_round_trip(int64_t(bits));
            break;
        }
    }
    return 0;
}
