//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"
#include "../core/vector.h"
#include "stencil.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// A template whose output is HTML: stencil's syntax and values, and every
// field written escaped for the place in the page where it lands, as Go's
// html/template writes it. The template's own text is read as HTML once,
// where the source is parsed, and each {{ }} is given the escaping of its
// context — text, an attribute, a URL, JavaScript, CSS:
//
//     txt::html_stencil page(R"(<a href="{{ url }}" title="{{ title }}">{{ name }}</a>)");
//     page.render(data);    // a javascript: URL becomes #ZgotmplZ, a quote &#34;
//
// The security model is html/template's: the template is trusted (it is
// code), the data is not. What a field writes cannot leave the context its
// template put it in: it cannot close an attribute or a string, open a tag
// or a script, or carry a scheme other than http, https or mailto into a
// URL. Where a value is trusted HTML, a URL, JavaScript or CSS, the template
// says so with the pipeline functions safe_html, safe_url, safe_attr,
// safe_js and safe_css, and the value is written unescaped in that context
// only; the data alone can never mark itself trusted.
//
// What the reading refuses, as Go refuses it: branches of an if, a with or
// a range that leave the page in different contexts, a source that ends
// inside a tag, an attribute, a script or a style, a URL whose part a
// field cannot tell (after branches that disagree), and a slash in a script
// that could start a division or a regular expression. HTML, JavaScript
// and CSS comments in the template's text are dropped from the page.
//
// Where it does not follow Go: an object in a script is written in the
// order its fields were set, where Go's JSON sorts the keys; a list or an
// object in an HTML context is written as stencil writes it, where Go
// writes its fmt form; entities in an attribute's text are decoded for the
// analysis by their numeric forms and the common names (amp lt gt quot apos
// nbsp), where Go knows every name.
namespace sgcl::txt {
    namespace detail {
        //----------------------------------------------------------------
        // The context of a point in the page
        //----------------------------------------------------------------
        enum HtmlState : uint8_t {
            HsText, HsTag, HsAttrName, HsAfterName, HsBeforeValue, HsComment, HsRcdata, HsAttr, HsUrl, HsSrcset,
            HsJs, HsJsDq, HsJsSq, HsJsTmpl, HsJsRegexp, HsJsBlockComment, HsJsLineComment,
            HsCss, HsCssDq, HsCssSq, HsCssDqUrl, HsCssSqUrl, HsCssUrl, HsCssBlockComment, HsCssLineComment,
            HsError,
        };

        enum HtmlDelim : uint8_t { HdNone, HdDouble, HdSingle, HdSpace };
        enum HtmlUrlPart : uint8_t { HuNone, HuPre, HuQuery, HuUnknown };
        enum HtmlJsCtx : uint8_t { HjRegexp, HjDiv, HjUnknown };
        enum HtmlAttr : uint8_t { HaNone, HaScript, HaScriptType, HaStyle, HaUrl, HaSrcset };
        enum HtmlElement : uint8_t { HeNone, HeScript, HeStyle, HeTextarea, HeTitle };

        struct HtmlCtx {
            uint8_t state = HsText;
            uint8_t delim = HdNone;
            uint8_t url = HuNone;
            uint8_t js = HjRegexp;
            uint8_t attr = HaNone;
            uint8_t element = HeNone;
            uint8_t braces = 0;          // template literals open: the depth of each ${ in `tmpl`
            uint8_t depth[8] = {};       // the braces open inside each
            bool script_js = true;       // a <script> whose type is JavaScript (or none)

            friend bool operator==(const HtmlCtx& a, const HtmlCtx& b) noexcept {
                if (a.state != b.state || a.delim != b.delim || a.url != b.url || a.js != b.js || a.attr != b.attr
                    || a.element != b.element || a.braces != b.braces || a.script_js != b.script_js) {
                    return false;
                }
                for (uint8_t i = 0; i < a.braces && i < 8; ++i) {
                    if (a.depth[i] != b.depth[i]) {
                        return false;
                    }
                }
                return true;
            }
        };

        // The escaping of a field: what the value is turned into in its
        // context (the inner), then for an attribute value its attribute's
        // escaping (the outer)
        enum HtmlEscape : uint8_t {
            HeHtml, HeRcdata, HeNospace, HeName, HeUrlFilter, HeUrlNormal, HeUrlQuery, HeSrcset,
            HeJsValue, HeJsString, HeJsRegexp, HeJsTemplate, HeCssValue, HeCssString, HeNothing, HeAttrHtml,
            HeCssStringPre, HeCssStringQuery,
        };
        enum HtmlOuter : uint8_t { HoNone, HoQuoted, HoUnquoted };
        // the trusted kinds of the safe_ functions
        enum HtmlTrust : uint8_t { HtNone, HtHtml, HtUrl, HtAttr, HtJs, HtCss };

        SGCL_INLINE_HOT constexpr uint32_t html_escape_code(uint8_t inner, uint8_t outer, uint8_t trust) noexcept {
            return uint32_t(inner) | uint32_t(outer) << 8 | uint32_t(trust) << 16;
        }

        //----------------------------------------------------------------
        // The escapers, each appending to a std::string
        //----------------------------------------------------------------
        inline void html_hex(std::string& out, uint32_t v, int digits) {
            static constexpr char hex[] = "0123456789abcdef";
            for (int k = digits - 1; k >= 0; --k) {
                out.push_back(hex[(v >> (4 * k)) & 15]);
            }
        }

        // keep_amp: trusted HTML, whose & begins its own references
        inline void escape_html_text(std::string& out, std::string_view s, bool keep_amp = false) {
            for (char c : s) {
                switch (c) {
                    case '\0': out.append("\xEF\xBF\xBD"); break;
                    case '"': out.append("&#34;"); break;
                    case '&': out.append(keep_amp ? "&" : "&amp;"); break;
                    case '\'': out.append("&#39;"); break;
                    case '+': out.append("&#43;"); break;
                    case '<': out.append("&lt;"); break;
                    case '>': out.append("&gt;"); break;
                    default: out.push_back(c);
                }
            }
        }

        // an unquoted attribute value: also the characters that would end it
        inline void escape_html_nospace(std::string& out, std::string_view s, bool keep_amp = false) {
            if (s.empty() && !keep_amp) {
                out.append("ZgotmplZ");
                return;
            }
            for (char c : s) {
                if (c == '&' && keep_amp) {
                    out.push_back(c);
                    continue;
                }
                switch (c) {
                    case '\0': out.append("&#xfffd;"); break;
                    case '\t': out.append("&#9;"); break;
                    case '\n': out.append("&#10;"); break;
                    case '\v': out.append("&#11;"); break;
                    case '\f': out.append("&#12;"); break;
                    case '\r': out.append("&#13;"); break;
                    case ' ': out.append("&#32;"); break;
                    case '"': out.append("&#34;"); break;
                    case '&': out.append("&amp;"); break;
                    case '\'': out.append("&#39;"); break;
                    case '+': out.append("&#43;"); break;
                    case '<': out.append("&lt;"); break;
                    case '=': out.append("&#61;"); break;
                    case '>': out.append("&gt;"); break;
                    case '`': out.append("&#96;"); break;
                    default: out.push_back(c);
                }
            }
        }

        inline char html_lower(char c) noexcept {
            return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
        }

        // the kind of an attribute by its name: what its value holds
        inline uint8_t html_attr_kind(std::string_view name) noexcept {
            std::string n;
            for (char c : name) {
                n.push_back(html_lower(c));
            }
            if (n.size() > 6 && n.compare(0, 6, "xmlns:") == 0) {
                return HaUrl;
            }
            size_t colon = n.find(':');
            if (colon != std::string::npos) {
                n.erase(0, colon + 1);
            } else if (n.size() > 5 && n.compare(0, 5, "data-") == 0) {
                n.erase(0, 5);
            }
            if (n.size() >= 2 && n[0] == 'o' && n[1] == 'n') {
                return HaScript;
            }
            if (n == "style") {
                return HaStyle;
            }
            if (n == "srcset") {
                return HaSrcset;
            }
            static constexpr const char* urls[] = {
                "action", "archive", "background", "cite", "classid", "codebase", "data", "formaction", "href",
                "icon", "longdesc", "manifest", "poster", "profile", "src", "usemap", "xmlns",
            };
            for (const char* u : urls) {
                if (n == u) {
                    return HaUrl;
                }
            }
            if (n.find("src") != std::string::npos || n.find("uri") != std::string::npos
                || n.find("url") != std::string::npos) {
                return HaUrl;
            }
            return HaNone;
        }

        // an attribute's name from data: ASCII letters and digits, a name
        // of no special kind; lowercased
        inline void filter_html_name(std::string& out, std::string_view s) {
            std::string n;
            for (char c : s) {
                char l = html_lower(c);
                if (!((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9'))) {
                    out.append("ZgotmplZ");
                    return;
                }
                n.push_back(l);
            }
            if (n.empty() || html_attr_kind(n) != HaNone) {
                out.append("ZgotmplZ");
                return;
            }
            out.append(n);
        }

        // a URL whose scheme is not http, https or mailto: #ZgotmplZ
        inline bool url_scheme_allowed(std::string_view s) noexcept {
            size_t colon = s.find(':');
            if (colon == std::string_view::npos || s.substr(0, colon).find('/') != std::string_view::npos) {
                return true;
            }
            std::string p;
            for (char c : s.substr(0, colon)) {
                p.push_back(html_lower(c));
            }
            return p == "http" || p == "https" || p == "mailto";
        }

        inline bool html_is_hex(char c) noexcept {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }

        // percent-encoding made valid: what a URL may hold is kept, a %
        // that begins an escape kept, the rest encoded
        inline void normalize_url(std::string& out, std::string_view s) {
            for (size_t i = 0; i < s.size(); ++i) {
                unsigned char c = (unsigned char)s[i];
                bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
                if (!keep) {
                    switch (c) {
                        case '-': case '.': case '_': case '~': case '!': case '#': case '$': case '&': case '*':
                        case '+': case ',': case '/': case ':': case ';': case '=': case '?': case '@': case '[':
                        case ']':
                            keep = true;
                            break;
                        case '%':
                            keep = i + 2 < s.size() && html_is_hex(s[i + 1]) && html_is_hex(s[i + 2]);
                            break;
                        default:
                            break;
                    }
                }
                if (keep) {
                    out.push_back(char(c));
                } else {
                    out.push_back('%');
                    html_hex(out, c, 2);
                }
            }
        }

        // a part of a query: everything but the unreserved encoded
        inline void escape_url_query(std::string& out, std::string_view s) {
            for (unsigned char c : s) {
                bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'
                    || c == '.' || c == '_' || c == '~';
                if (keep) {
                    out.push_back(char(c));
                } else {
                    out.push_back('%');
                    html_hex(out, c, 2);
                }
            }
        }

        inline bool html_space(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
        }

        // the candidates of a srcset, each URL filtered and normalized, each
        // descriptor of letters, digits and dots only
        inline void filter_srcset(std::string& out, std::string_view s, bool trusted) {
            if (trusted) {
                normalize_url(out, s);   // a trusted URL: one, normalized
                return;
            }
            size_t b = 0;
            while (true) {
                size_t e = s.find(',', b);
                std::string_view cand = s.substr(b, e == std::string_view::npos ? std::string_view::npos : e - b);
                size_t i = 0;
                while (i < cand.size() && html_space(cand[i])) {
                    ++i;
                }
                size_t u = i;
                while (i < cand.size() && !html_space(cand[i])) {
                    ++i;
                }
                std::string_view url = cand.substr(u, i - u), rest = cand.substr(i);
                bool ok = trusted || url_scheme_allowed(url);
                for (char c : rest) {
                    if (!(html_space(c) || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                          || c == '.')) {
                        ok = false;
                    }
                }
                if (ok) {
                    out.append(cand.substr(0, u));
                    normalize_url(out, url);
                    out.append(rest);
                } else {
                    out.append("#ZgotmplZ");
                }
                if (e == std::string_view::npos) {
                    break;
                }
                out.push_back(',');
                b = e + 1;
            }
        }

        // the code point at i of UTF-8 text (a bad byte U+FFFD, its length 1)
        inline char32_t html_decode(std::string_view s, size_t i, size_t& len) noexcept {
            unsigned char c = (unsigned char)s[i];
            if (c < 0x80) {
                len = 1;
                return c;
            }
            int n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            if (n == 1 || i + size_t(n) > s.size()) {
                len = 1;
                return 0xFFFD;
            }
            char32_t v = c & (0x7F >> n);
            for (int k = 1; k < n; ++k) {
                unsigned char d = (unsigned char)s[i + k];
                if ((d & 0xC0) != 0x80) {
                    len = 1;
                    return 0xFFFD;
                }
                v = v << 6 | (d & 0x3F);
            }
            len = size_t(n);
            return v;
        }

        // inside a JavaScript string, regular expression or template literal
        inline void escape_js_string(std::string& out, std::string_view s, uint8_t kind) {
            if (kind == HeJsRegexp && s.empty()) {
                out.append("(?:)");   // an empty pattern would make a comment of //
                return;
            }
            for (size_t i = 0; i < s.size();) {
                size_t len;
                char32_t c = html_decode(s, i, len);
                if (c == 0x2028 || c == 0x2029) {
                    out.append(c == 0x2028 ? "\\u2028" : "\\u2029");
                    i += len;
                    continue;
                }
                if (c >= 0x80) {
                    out.append(s.substr(i, len));
                    i += len;
                    continue;
                }
                i += len;
                switch (c) {
                    case '\t': out.append("\\t"); continue;
                    case '\n': out.append("\\n"); continue;
                    case '\f': out.append("\\f"); continue;
                    case '\r': out.append("\\r"); continue;
                    case '"': case '&': case '\'': case '+': case '<': case '>':
                        out.append("\\u00");
                        html_hex(out, uint32_t(c), 2);
                        continue;
                    case '`':
                        if (kind == HeJsRegexp) {
                            out.push_back('`');
                        } else {
                            out.append("\\u0060");
                        }
                        continue;
                    case '/': out.append("\\/"); continue;
                    case '\\': out.append("\\\\"); continue;
                    default: break;
                }
                if (c < 0x20) {
                    out.append("\\u00");
                    html_hex(out, uint32_t(c), 2);
                    continue;
                }
                if (kind == HeJsRegexp) {
                    switch (c) {
                        case '$': case '(': case ')': case '*': case '-': case '.': case '?': case '[': case ']':
                        case '^': case '{': case '|': case '}':
                            out.push_back('\\');
                            out.push_back(char(c));
                            continue;
                        default: break;
                    }
                }
                if (kind == HeJsTemplate && (c == '$' || c == '{' || c == '}')) {
                    out.append("\\u00");
                    html_hex(out, uint32_t(c), 2);
                    continue;
                }
                out.push_back(char(c));
            }
        }

        // a JSON string, as a JavaScript value
        inline void json_string(std::string& out, std::string_view s) {
            out.push_back('"');
            for (size_t i = 0; i < s.size();) {
                size_t len;
                char32_t c = html_decode(s, i, len);
                bool bad = c == 0xFFFD && !(len == 3 && s.substr(i, 3) == "\xEF\xBF\xBD");
                if (bad) {
                    out.append("\\ufffd");
                    i += len;
                    continue;
                }
                if (c == 0x2028 || c == 0x2029) {
                    out.append(c == 0x2028 ? "\\u2028" : "\\u2029");
                    i += len;
                    continue;
                }
                if (c >= 0x80) {
                    out.append(s.substr(i, len));
                    i += len;
                    continue;
                }
                i += len;
                switch (c) {
                    case '"': out.append("\\\""); continue;
                    case '\\': out.append("\\\\"); continue;
                    case '\b': out.append("\\b"); continue;
                    case '\f': out.append("\\f"); continue;
                    case '\n': out.append("\\n"); continue;
                    case '\r': out.append("\\r"); continue;
                    case '\t': out.append("\\t"); continue;
                    case '<': case '>': case '&':
                        out.append("\\u00");
                        html_hex(out, uint32_t(c), 2);
                        continue;
                    default: break;
                }
                if (c < 0x20) {
                    out.append("\\u00");
                    html_hex(out, uint32_t(c), 2);
                    continue;
                }
                out.push_back(char(c));
            }
            out.push_back('"');
        }

        // a double as Go's JSON writes it: the shortest digits, an
        // exponent below 1e-6 and from 1e21
        inline void json_number(std::string& out, double d) {
            if (!std::isfinite(d)) {
                out.append("null");
                return;
            }
            char buf[64];
            double a = std::fabs(d);
            std::to_chars_result r;
            if (a != 0 && (a < 1e-6 || a >= 1e21)) {
                r = std::to_chars(buf, buf + sizeof buf, d, std::chars_format::scientific);
                std::string t(buf, r.ptr);
                size_t e = t.find('e');
                // e-07 -> e-7
                if (e != std::string::npos && e + 2 < t.size() && t[e + 2] == '0') {
                    t.erase(e + 2, 1);
                }
                out.append(t);
                return;
            }
            r = std::to_chars(buf, buf + sizeof buf, d, std::chars_format::fixed);
            out.append(buf, r.ptr);
        }

        inline void json_value(std::string& out, const value& v, int depth);

        inline void json_value(std::string& out, const value& v, int depth) {
            if (depth > 64) {
                out.append("null");
                return;
            }
            switch (v.kind()) {
                case value_kind::none: out.append("null"); return;
                case value_kind::boolean: out.append(v.truthy() ? "true" : "false"); return;
                case value_kind::integer: {
                    char buf[24];
                    auto r = std::to_chars(buf, buf + sizeof buf, *value_reach::integer_of(v));
                    out.append(buf, r.ptr);
                    return;
                }
                case value_kind::real: json_number(out, *value_reach::real_of(v)); return;
                case value_kind::text: json_string(out, v.text()->view()); return;
                case value_kind::list: {
                    out.push_back('[');
                    const value_list* l = value_reach::list_of(v);
                    for (size_t i = 0; l && i < l->items.size(); ++i) {
                        if (i) {
                            out.push_back(',');
                        }
                        json_value(out, l->items[i], depth + 1);
                    }
                    out.push_back(']');
                    return;
                }
                default: {
                    out.push_back('{');
                    const value_object* o = value_reach::object_of(v);
                    bool first = true;
                    if (o) {
                        for (const auto& f : o->fields) {
                            if (!first) {
                                out.push_back(',');
                            }
                            first = false;
                            json_string(out, f.first.view());
                            out.push_back(':');
                            json_value(out, f.second, depth + 1);
                        }
                    }
                    out.push_back('}');
                    return;
                }
            }
        }

        // a value in a script's expression: JSON, padded with spaces where
        // it could run into the tokens around it
        inline void escape_js_value(std::string& out, const value& v) {
            std::string j;
            json_value(j, v, 0);
            bool pad = !j.empty() && j[0] != '"' && j[0] != '[' && j[0] != '{';
            if (pad) {
                out.push_back(' ');
            }
            out.append(j);
            if (pad) {
                out.push_back(' ');
            }
        }

        inline bool css_space(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
        }

        // inside a CSS string or url(): \hex escapes, a space after one
        // where the next character could be read as part of it
        inline void escape_css_string(std::string& out, std::string_view s) {
            for (size_t i = 0; i < s.size(); ++i) {
                char c = s[i];
                const char* rep = nullptr;
                switch (c) {
                    case '\0': rep = "\\0"; break;
                    case '\t': rep = "\\9"; break;
                    case '\n': rep = "\\a"; break;
                    case '\f': rep = "\\c"; break;
                    case '\r': rep = "\\d"; break;
                    case '"': rep = "\\22"; break;
                    case '&': rep = "\\26"; break;
                    case '\'': rep = "\\27"; break;
                    case '(': rep = "\\28"; break;
                    case ')': rep = "\\29"; break;
                    case '+': rep = "\\2b"; break;
                    case '/': rep = "\\2f"; break;
                    case ':': rep = "\\3a"; break;
                    case ';': rep = "\\3b"; break;
                    case '<': rep = "\\3c"; break;
                    case '>': rep = "\\3e"; break;
                    case '\\': out.append("\\\\"); continue;
                    case '{': rep = "\\7b"; break;
                    case '}': rep = "\\7d"; break;
                    default: break;
                }
                if (!rep) {
                    out.push_back(c);
                    continue;
                }
                out.append(rep);
                if (i + 1 == s.size() || html_is_hex(s[i + 1]) || css_space(s[i + 1])) {
                    out.push_back(' ');
                }
            }
        }

        // a CSS value: its escapes decoded, kept only when it holds nothing
        // that could leave the value (a quote, a bracket, a semicolon, a
        // slash, an at) and no expression() or -moz-binding
        inline void filter_css_value(std::string& out, std::string_view s) {
            std::string d;
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] != '\\') {
                    d.push_back(s[i]);
                    continue;
                }
                size_t j = i + 1;
                uint32_t cp = 0;
                int n = 0;
                while (j < s.size() && n < 6 && html_is_hex(s[j])) {
                    char h = s[j];
                    cp = cp * 16 + uint32_t(h <= '9' ? h - '0' : (h | 32) - 'a' + 10);
                    ++j;
                    ++n;
                }
                if (n == 0) {
                    if (j < s.size()) {
                        d.push_back(s[j]);
                        i = j;
                    } else {
                        i = j - 1;
                    }
                    continue;
                }
                if (j < s.size() && css_space(s[j])) {
                    ++j;
                }
                if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
                    cp = 0xFFFD;
                }
                if (cp < 0x80) {
                    d.push_back(char(cp));
                } else if (cp < 0x800) {
                    d.push_back(char(0xC0 | cp >> 6));
                    d.push_back(char(0x80 | (cp & 63)));
                } else if (cp < 0x10000) {
                    d.push_back(char(0xE0 | cp >> 12));
                    d.push_back(char(0x80 | (cp >> 6 & 63)));
                    d.push_back(char(0x80 | (cp & 63)));
                } else {
                    d.push_back(char(0xF0 | cp >> 18));
                    d.push_back(char(0x80 | (cp >> 12 & 63)));
                    d.push_back(char(0x80 | (cp >> 6 & 63)));
                    d.push_back(char(0x80 | (cp & 63)));
                }
                i = j - 1;
            }
            std::string letters;
            for (char c : d) {
                switch (c) {
                    case '\0': case '"': case '\'': case '(': case ')': case '/': case ';': case '<': case '>':
                    case '@': case '[': case ']': case '`': case '{': case '}':
                        out.append("ZgotmplZ");
                        return;
                    default: break;
                }
                char l = html_lower(c);
                if (l >= 'a' && l <= 'z') {
                    letters.push_back(l);
                }
            }
            if (letters.find("expression") != std::string::npos || letters.find("mozbinding") != std::string::npos) {
                out.append("ZgotmplZ");
                return;
            }
            out.append(d);
        }

        // the text of trusted HTML without its tags, comments and the
        // content of its scripts and styles: what an attribute may hold of it
        inline std::string strip_html_tags(std::string_view s) {
            std::string out;
            size_t i = 0;
            auto letter = [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); };
            while (i < s.size()) {
                char ch = s[i];
                if (ch != '<') {
                    out.push_back(ch);
                    ++i;
                    continue;
                }
                if (s.substr(i, 4) == "<!--") {
                    size_t e = s.find("-->", i + 4);
                    i = e == std::string_view::npos ? s.size() : e + 3;
                    continue;
                }
                bool end = i + 1 < s.size() && s[i + 1] == '/';
                size_t b = i + (end ? 2 : 1);
                if (b >= s.size() || !letter(s[b])) {
                    out.push_back(ch);
                    ++i;
                    continue;
                }
                size_t j = b;
                while (j < s.size() && (letter(s[j]) || (s[j] >= '0' && s[j] <= '9') || s[j] == '-' || s[j] == ':')) {
                    ++j;
                }
                std::string name;
                for (size_t k = b; k < j; ++k) {
                    name.push_back(html_lower(s[k]));
                }
                // to the tag's end, past quoted values
                char quote = 0;
                while (j < s.size() && (quote || s[j] != '>')) {
                    if (quote ? s[j] == quote : (s[j] == '"' || s[j] == '\'')) {
                        quote = quote ? 0 : s[j];
                    }
                    ++j;
                }
                i = j < s.size() ? j + 1 : s.size();
                if (!end && (name == "script" || name == "style")) {
                    std::string close = "</" + name;
                    size_t k = i;
                    while (k < s.size()) {
                        if (s[k] == '<' && k + close.size() <= s.size()) {
                            bool same = true;
                            for (size_t m = 0; m < close.size() && same; ++m) {
                                same = html_lower(s[k + m]) == close[m];
                            }
                            if (same) {
                                break;
                            }
                        }
                        ++k;
                    }
                    i = k;
                }
            }
            return out;
        }

        //----------------------------------------------------------------
        // Writing a value in its context
        //----------------------------------------------------------------
        inline void html_value_text(std::string& text, const value& v, const format_spec& spec,
                                    std::string_view nested) {
            if (v.kind() == value_kind::none) {
                return;
            }
            char room[256];
            format_sink first(room, sizeof room);
            v.write(first, spec, nested);
            if (first.size() <= sizeof room) {
                text.assign(room, first.size());
                return;
            }
            text.resize(first.size());
            format_sink again(text.data(), text.size());
            v.write(again, spec, nested);
            text.resize(again.size() < text.size() ? again.size() : text.size());
        }

        inline void html_write(format_sink& out, const value& v, const format_spec& spec, std::string_view nested,
                               uint32_t code) {
            uint8_t inner = uint8_t(code), outer = uint8_t(code >> 8), trust = uint8_t(code >> 16);
            std::string text, done;
            if (inner == HeNothing) {
                return;
            }
            if (inner == HeJsValue) {
                if (trust == HtJs) {   // trusted JavaScript: only where an expression is
                    html_value_text(done, v, spec, nested);
                } else {
                    escape_js_value(done, v);
                }
            } else {
                html_value_text(text, v, spec, nested);
                switch (inner) {
                    case HeHtml:
                        if (trust == HtHtml) {   // trusted HTML: only in text (html_field passes it nowhere else)
                            done = text;
                        } else {
                            escape_html_text(done, text);
                        }
                        break;
                    case HeRcdata:
                        escape_html_text(done, text, trust == HtHtml);   // trusted HTML keeps its references
                        break;
                    case HeNospace:
                        if (trust == HtHtml && !text.empty()) {
                            escape_html_nospace(done, strip_html_tags(text), true);
                        } else if (trust == HtHtml) {
                            done = "ZgotmplZ";
                        } else {
                            escape_html_nospace(done, text);
                        }
                        break;
                    case HeAttrHtml:
                        if (trust == HtHtml) {
                            escape_html_text(done, strip_html_tags(text), true);
                        } else {
                            escape_html_text(done, text);
                        }
                        break;
                    case HeName:
                        if (trust == HtAttr) {
                            done = text;
                        } else {
                            filter_html_name(done, text);
                        }
                        break;
                    case HeUrlFilter:
                        if (trust != HtUrl && !url_scheme_allowed(text)) {
                            done = "#ZgotmplZ";
                        } else {
                            normalize_url(done, text);
                        }
                        break;
                    case HeUrlNormal: normalize_url(done, text); break;
                    case HeUrlQuery:
                        if (trust == HtUrl) {
                            normalize_url(done, text);   // a trusted URL keeps its own structure
                        } else {
                            escape_url_query(done, text);
                        }
                        break;
                    case HeSrcset: filter_srcset(done, text, trust == HtUrl); break;
                    case HeJsString: case HeJsRegexp: case HeJsTemplate:
                        escape_js_string(done, text, inner);
                        break;
                    case HeCssValue:
                        if (trust == HtCss) {
                            done = text;
                        } else {
                            filter_css_value(done, text);
                        }
                        break;
                    case HeCssString:
                        // a CSS string may be read as a URL (@import, url()): filtered as one
                        // at its start, its query escaped as one
                        if (trust != HtUrl && !url_scheme_allowed(text)) {
                            done = "#ZgotmplZ";
                        } else {
                            escape_css_string(done, text);
                        }
                        break;
                    case HeCssStringPre: escape_css_string(done, text); break;
                    case HeCssStringQuery: {
                        std::string q;
                        escape_url_query(q, text);
                        escape_css_string(done, q);
                        break;
                    }
                    default: break;
                }
            }
            if (outer == HoQuoted) {
                std::string a;
                escape_html_text(a, done);
                out.put(a.data(), a.size());
            } else if (outer == HoUnquoted) {
                std::string a;
                escape_html_nospace(a, done);
                out.put(a.data(), a.size());
            } else {
                out.put(done.data(), done.size());
            }
        }

        //----------------------------------------------------------------
        // The reading of the template's text: the context after a run of
        // it, and the text written (comments dropped, a lone < escaped)
        //----------------------------------------------------------------
        struct HtmlScanner {
            HtmlCtx c;
            std::string out;              // what the run writes
            const char* error = nullptr;
            // the name of the attribute being read, and the tag's
            std::string attr_name;
            std::string tag_name;
            bool end_tag = false;
            std::string type_value;       // a <script>'s type, read in its value
            bool reading_type = false;

            static bool letter(char ch) noexcept {
                return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
            }

            static bool same_ci(std::string_view a, std::string_view b) noexcept {
                if (a.size() != b.size()) {
                    return false;
                }
                for (size_t i = 0; i < a.size(); ++i) {
                    if (html_lower(a[i]) != html_lower(b[i])) {
                        return false;
                    }
                }
                return true;
            }

            static bool js_type(std::string_view t) noexcept {
                size_t semi = t.find(';');
                t = t.substr(0, semi);
                while (!t.empty() && html_space(t.front())) {
                    t.remove_prefix(1);
                }
                while (!t.empty() && html_space(t.back())) {
                    t.remove_suffix(1);
                }
                std::string l;
                for (char ch : t) {
                    l.push_back(html_lower(ch));
                }
                static constexpr const char* types[] = {
                    "", "application/ecmascript", "application/javascript", "application/json",
                    "application/ld+json", "application/x-ecmascript", "application/x-javascript", "module",
                    "text/ecmascript", "text/javascript", "text/javascript1.0", "text/javascript1.1",
                    "text/javascript1.2", "text/javascript1.3", "text/javascript1.4", "text/javascript1.5",
                    "text/jscript", "text/livescript", "text/x-ecmascript", "text/x-javascript",
                };
                for (const char* x : types) {
                    if (l == x) {
                        return true;
                    }
                }
                return false;
            }

            // the end tag of the element whose content this is, at s[i]
            size_t element_end(std::string_view s, size_t i) const noexcept {
                const char* name = c.element == HeScript ? "script" : c.element == HeStyle ? "style"
                    : c.element == HeTextarea ? "textarea" : "title";
                size_t n = std::char_traits<char>::length(name);
                if (s.size() - i < n + 2 || s[i] != '<' || s[i + 1] != '/') {
                    return std::string_view::npos;
                }
                if (!same_ci(s.substr(i + 2, n), name)) {
                    return std::string_view::npos;
                }
                if (i + 2 + n < s.size()) {
                    char after = s[i + 2 + n];
                    if (!(html_space(after) || after == '/' || after == '>')) {
                        return std::string_view::npos;
                    }
                }
                return i;
            }

            // back in text: nothing of a tag, a value or a script left over
            void to_text() noexcept {
                c = HtmlCtx();
            }

            void enter_element_content() {
                if (c.element == HeScript) {
                    c.state = c.script_js ? HsJs : HsText;
                    if (!c.script_js) {
                        c.element = HeNone;
                    }
                    c.js = HjRegexp;
                } else if (c.element == HeStyle) {
                    c.state = HsCss;
                } else if (c.element == HeTextarea || c.element == HeTitle) {
                    c.state = HsRcdata;
                } else {
                    c.state = HsText;
                }
            }

            // a run of text from the start of the template's piece
            void run(std::string_view s) {
                size_t i = 0;
                while (i < s.size() && !error) {
                    i = step(s, i);
                }
            }

            // one transition; the index after what it consumed
            size_t step(std::string_view s, size_t i) {
                switch (c.state) {
                    case HsText: return text(s, i);
                    case HsComment: return comment(s, i);
                    case HsRcdata: return rcdata(s, i);
                    case HsTag: return tag(s, i);
                    case HsAttrName: return attr_name_run(s, i);
                    case HsAfterName: return after_name(s, i);
                    case HsBeforeValue: return before_value(s, i);
                    default: return in_value_or_element(s, i);
                }
            }

            size_t text(std::string_view s, size_t i) {
                size_t lt = s.find('<', i);
                if (lt == std::string_view::npos) {
                    out.append(s.substr(i));
                    return s.size();
                }
                out.append(s.substr(i, lt - i));
                if (s.substr(lt, 4) == "<!--") {
                    c.state = HsComment;
                    return lt + 4;
                }
                char next = lt + 1 < s.size() ? s[lt + 1] : '\0';
                bool end = next == '/' && lt + 2 < s.size() && letter(s[lt + 2]);
                if (letter(next) || end) {
                    size_t j = lt + (end ? 2 : 1);
                    size_t b = j;
                    // a tag's name: letters, digits, - and : (what follows is the tag's)
                    while (j < s.size() && (letter(s[j]) || (s[j] >= '0' && s[j] <= '9') || s[j] == '-' || s[j] == ':')) {
                        ++j;
                    }
                    tag_name.assign(s.substr(b, j - b));
                    end_tag = end;
                    out.append(s.substr(lt, j - lt));
                    c.state = HsTag;
                    c.element = HeNone;
                    c.script_js = true;
                    if (!end) {
                        if (same_ci(tag_name, "script")) {
                            c.element = HeScript;
                        } else if (same_ci(tag_name, "style")) {
                            c.element = HeStyle;
                        } else if (same_ci(tag_name, "textarea")) {
                            c.element = HeTextarea;
                        } else if (same_ci(tag_name, "title")) {
                            c.element = HeTitle;
                        }
                    }
                    return j;
                }
                if (next == '!' && s.size() - lt >= 9 && same_ci(s.substr(lt + 2, 7), "doctype")) {
                    out.push_back('<');
                    return lt + 1;
                }
                out.append("&lt;");
                return lt + 1;
            }

            size_t comment(std::string_view s, size_t i) {
                size_t e = s.find("-->", i);
                if (e == std::string_view::npos) {
                    return s.size();
                }
                c.state = HsText;
                return e + 3;
            }

            size_t rcdata(std::string_view s, size_t i) {
                for (size_t k = i; k < s.size(); ++k) {
                    if (s[k] != '<') {
                        out.push_back(s[k]);
                        continue;
                    }
                    if (element_end(s, k) != std::string_view::npos) {
                        to_text();
                        return k;
                    }
                    if (s.size() - k >= 9 && s[k + 1] == '!' && same_ci(s.substr(k + 2, 7), "doctype")) {
                        out.push_back('<');
                        continue;
                    }
                    out.append("&lt;");   // a < that is not the element's end tag
                }
                return s.size();
            }

            size_t tag(std::string_view s, size_t i) {
                while (i < s.size() && html_space(s[i])) {
                    out.push_back(s[i++]);
                }
                if (i >= s.size()) {
                    return i;
                }
                if (s[i] == '>') {
                    out.push_back('>');
                    if (end_tag) {
                        to_text();
                    } else {
                        bool js_script = c.script_js;
                        uint8_t element = c.element;
                        to_text();
                        c.element = element;
                        c.script_js = js_script;
                        enter_element_content();
                    }
                    end_tag = false;
                    return i + 1;
                }
                if (s[i] == '"' || s[i] == '\'' || s[i] == '<' || s[i] == '=') {
                    error = "a quote, < or = where an attribute's name was expected";
                    return s.size();
                }
                if (s[i] == '/') {
                    out.push_back('/');
                    return i + 1;
                }
                // an attribute's name
                size_t b = i;
                while (i < s.size() && !html_space(s[i]) && s[i] != '=' && s[i] != '>' && s[i] != '/') {
                    if (s[i] == '"' || s[i] == '\'' || s[i] == '<') {
                        error = "a quote or < in an attribute's name";
                        return s.size();
                    }
                    ++i;
                }
                if (i == b) {
                    out.push_back(s[i]);   // a stray character of the tag
                    return i + 1;
                }
                attr_name.assign(s.substr(b, i - b));
                out.append(s.substr(b, i - b));
                c.state = i < s.size() ? HsAfterName : HsAttrName;
                set_attr_kind();
                return i;
            }

            void set_attr_kind() {
                c.attr = html_attr_kind(attr_name);
                if (c.element == HeScript && same_ci(attr_name, "type")) {
                    c.attr = HaScriptType;
                }
            }

            size_t attr_name_run(std::string_view s, size_t i) {
                size_t b = i;
                while (i < s.size() && !html_space(s[i]) && s[i] != '=' && s[i] != '>' && s[i] != '/') {
                    if (s[i] == '"' || s[i] == '\'' || s[i] == '<') {
                        error = "a quote or < in an attribute's name";
                        return s.size();
                    }
                    ++i;
                }
                attr_name.append(s.substr(b, i - b));
                out.append(s.substr(b, i - b));
                set_attr_kind();
                if (i < s.size()) {
                    c.state = HsAfterName;
                }
                return i;
            }

            size_t after_name(std::string_view s, size_t i) {
                while (i < s.size() && html_space(s[i])) {
                    out.push_back(s[i++]);
                }
                if (i >= s.size()) {
                    return i;
                }
                if (s[i] == '=') {
                    out.push_back('=');
                    c.state = HsBeforeValue;
                    return i + 1;
                }
                c.state = HsTag;
                c.attr = HaNone;
                return i;
            }

            size_t before_value(std::string_view s, size_t i) {
                while (i < s.size() && html_space(s[i])) {
                    out.push_back(s[i++]);
                }
                if (i >= s.size()) {
                    return i;
                }
                if (s[i] == '"' || s[i] == '\'') {
                    out.push_back(s[i]);
                    c.delim = s[i] == '"' ? HdDouble : HdSingle;
                    start_value();
                    return i + 1;
                }
                if (s[i] == '>') {
                    c.state = HsTag;
                    c.attr = HaNone;
                    return i;
                }
                c.delim = HdSpace;
                start_value();
                return i;
            }

            void start_value() {
                type_value.clear();
                reading_type = c.attr == HaScriptType;
                switch (c.attr) {
                    case HaScript: c.state = HsJs; c.js = HjRegexp; break;
                    case HaStyle: c.state = HsCss; break;
                    case HaUrl: c.state = HsUrl; c.url = HuNone; break;
                    case HaSrcset: c.state = HsSrcset; break;
                    default: c.state = HsAttr; break;
                }
            }

            void end_value() {
                if (reading_type) {
                    c.script_js = js_type(type_value);
                    reading_type = false;
                }
                c.state = HsTag;
                c.attr = HaNone;
                c.delim = HdNone;
                c.url = HuNone;
                c.js = HjRegexp;
                c.braces = 0;
            }

            // decoded text of an attribute value, for the analysis: numeric
            // references and the common names
            static std::string decode_entities(std::string_view s) {
                std::string d;
                for (size_t i = 0; i < s.size(); ++i) {
                    if (s[i] != '&') {
                        d.push_back(s[i]);
                        continue;
                    }
                    size_t semi = s.find(';', i);
                    if (semi == std::string_view::npos || semi - i > 10) {
                        d.push_back('&');
                        continue;
                    }
                    std::string_view name = s.substr(i + 1, semi - i - 1);
                    uint32_t cp = 0;
                    bool ok = true;
                    if (!name.empty() && name[0] == '#') {
                        bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
                        std::string_view digits = name.substr(hex ? 2 : 1);
                        if (digits.empty()) {
                            ok = false;
                        }
                        for (char ch : digits) {
                            if (hex ? !html_is_hex(ch) : !(ch >= '0' && ch <= '9')) {
                                ok = false;
                                break;
                            }
                            cp = cp * (hex ? 16 : 10) + uint32_t(ch <= '9' ? ch - '0' : (ch | 32) - 'a' + 10);
                            if (cp > 0x10FFFF) {
                                cp = 0xFFFD;
                            }
                        }
                    } else if (name == "amp") {
                        cp = '&';
                    } else if (name == "lt") {
                        cp = '<';
                    } else if (name == "gt") {
                        cp = '>';
                    } else if (name == "quot") {
                        cp = '"';
                    } else if (name == "apos") {
                        cp = '\'';
                    } else if (name == "nbsp") {
                        cp = 0xA0;
                    } else {
                        ok = false;
                    }
                    if (!ok) {
                        d.push_back('&');
                        continue;
                    }
                    if (cp < 0x80) {
                        d.push_back(char(cp));
                    } else {
                        d.append("\xC2\xA0");   // the analysis needs only that it is not ASCII
                    }
                    i = semi;
                }
                return d;
            }

            size_t in_value_or_element(std::string_view s, size_t i) {
                if (c.delim != HdNone) {
                    // the value runs to its delimiter
                    size_t e = i;
                    if (c.delim == HdSpace) {
                        while (e < s.size() && !html_space(s[e]) && s[e] != '>') {
                            ++e;
                        }
                    } else {
                        e = s.find(c.delim == HdDouble ? '"' : '\'', i);
                        if (e == std::string_view::npos) {
                            e = s.size();
                        }
                    }
                    std::string_view raw = s.substr(i, e - i);
                    if (c.delim == HdSpace && raw.find_first_of("\"'<=`") != std::string_view::npos) {
                        error = "a quote, <, = or ` in an unquoted attribute value";
                        return s.size();
                    }
                    if (reading_type) {
                        type_value.append(raw);
                    }
                    std::string decoded = decode_entities(raw);
                    // the analysis over the decoded text, the raw text written
                    std::string saved;
                    saved.swap(out);
                    inner(decoded);
                    out.swap(saved);
                    out.append(raw);
                    if (e < s.size()) {
                        if (c.delim != HdSpace) {
                            out.push_back(s[e]);
                            ++e;
                        }
                        end_value();
                    }
                    return e;
                }
                // an element's content, a transition at a time, each up to
                // the element's end tag — except inside a string, a pattern or
                // a comment of a script, where the end tag is text (a < of it
                // in a string written \x3C), as Go's html/template reads it
                std::string_view rest = s.substr(i);
                size_t pos = 0;
                while (pos < rest.size() && !error) {
                    bool literal = c.state == HsJsDq || c.state == HsJsSq || c.state == HsJsTmpl
                        || c.state == HsJsRegexp || c.state == HsJsLineComment || c.state == HsJsBlockComment;
                    size_t cut = rest.size();
                    if (!(literal && c.element == HeScript)) {
                        for (size_t j = pos; j < rest.size(); ++j) {
                            if (rest[j] == '<' && element_end(rest, j) != std::string_view::npos) {
                                cut = j;
                                break;
                            }
                        }
                    }
                    if (cut == pos) {
                        if ((c.state == HsCssBlockComment || c.state == HsJsBlockComment) && i + pos > 0) {
                            out.push_back(' ');   // the comment the end tag cuts inside a run: one more space, as Go writes
                        }
                        to_text();
                        return i + pos;
                    }
                    pos += inner_step(rest.substr(pos, cut - pos));
                }
                return s.size();
            }

            //------------------------------------------------------------
            // JavaScript, CSS, URLs and plain attribute text
            //------------------------------------------------------------
            static bool ident_char(char ch) noexcept {
                return letter(ch) || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$' || (unsigned char)ch >= 0x80;
            }

            // the context of a slash after the text so far (its last token)
            static uint8_t js_after(std::string_view written) noexcept {
                size_t e = written.size();
                while (e > 0 && (html_space(written[e - 1]) || written[e - 1] == '\v')) {
                    --e;
                }
                if (e == 0) {
                    return HjRegexp;
                }
                char last = written[e - 1];
                if (last == '+' || last == '-') {
                    // ++ and -- end an expression; a lone + or - begins one
                    size_t k = e - 1;
                    while (k > 0 && written[k - 1] == last) {
                        --k;
                    }
                    return (e - k) % 2 == 0 ? HjDiv : HjRegexp;
                }
                if (last == ')' || last == ']') {
                    return HjDiv;
                }
                if (last == '.' && e >= 2 && written[e - 2] >= '0' && written[e - 2] <= '9') {
                    return HjDiv;
                }
                if (!ident_char(last)) {
                    return HjRegexp;
                }
                size_t b = e;
                while (b > 0 && ident_char(written[b - 1])) {
                    --b;
                }
                std::string_view word = written.substr(b, e - b);
                static constexpr const char* keywords[] = {
                    "break", "case", "continue", "delete", "do", "else", "finally", "in", "instanceof", "return",
                    "throw", "try", "typeof", "void",
                };
                for (const char* kw : keywords) {
                    if (word == kw) {
                        return HjRegexp;
                    }
                }
                return HjDiv;
            }

            std::string js_tokens;   // the significant JavaScript text so far, for js_after

            void inner(std::string_view s) {
                size_t i = 0;
                while (i < s.size() && !error) {
                    i += inner_step(s.substr(i));
                }
            }

            // one transition over s: what it consumed (nothing, when only
            // the state changed)
            size_t inner_step(std::string_view s) {
                size_t i = 0;
                uint8_t before = c.state;
                {
                    switch (c.state) {
                        case HsAttr: out.append(s.substr(i)); i = s.size(); break;
                        case HsUrl: i = url(s, i); break;
                        case HsSrcset: out.append(s.substr(i)); i = s.size(); break;
                        case HsJs: i = js(s, i); break;
                        case HsJsDq: case HsJsSq: case HsJsTmpl: i = js_string(s, i); break;
                        case HsJsRegexp: i = js_regexp(s, i); break;
                        case HsJsBlockComment: {
                            // a transition in a comment writes one space, a line
                            // break when what it read holds one (ES5 7.4)
                            size_t e = s.find("*/", i);
                            size_t to = e == std::string_view::npos ? s.size() : e + 2;
                            std::string_view read = s.substr(i, to - i);
                            bool line = read.find_first_of("\n\r") != std::string_view::npos
                                || read.find("\xE2\x80\xA8") != std::string_view::npos
                                || read.find("\xE2\x80\xA9") != std::string_view::npos;
                            if (c.delim == HdNone) {
                                out.push_back(line ? '\n' : ' ');
                            }
                            if (e != std::string_view::npos) {
                                c.state = HsJs;
                            }
                            i = to;
                            break;
                        }
                        case HsJsLineComment: {
                            size_t e = i;
                            while (e < s.size() && s[e] != '\n' && s[e] != '\r') {
                                ++e;
                            }
                            if (e < s.size()) {
                                c.state = HsJs;
                            }
                            i = e;
                            break;
                        }
                        case HsCss: i = css(s, i); break;
                        case HsCssDq: case HsCssSq: i = css_string(s, i); break;
                        case HsCssDqUrl: case HsCssSqUrl: case HsCssUrl: i = css_url(s, i); break;
                        case HsCssBlockComment: {
                            size_t e = s.find("*/", i);
                            if (c.delim == HdNone) {
                                out.push_back(' ');
                            }
                            if (e == std::string_view::npos) {
                                i = s.size();
                            } else {
                                c.state = HsCss;
                                i = e + 2;
                            }
                            break;
                        }
                        case HsCssLineComment: {
                            size_t e = i;
                            while (e < s.size() && s[e] != '\n' && s[e] != '\r' && s[e] != '\f') {
                                ++e;
                            }
                            if (e < s.size()) {
                                c.state = HsCss;
                            }
                            i = e;
                            break;
                        }
                        default: out.append(s.substr(i)); i = s.size(); break;
                    }
                }
                return i > 0 || c.state != before ? i : s.size();
            }

            size_t url(std::string_view s, size_t i) {
                for (; i < s.size(); ++i) {
                    char ch = s[i];
                    if (c.url == HuNone) {
                        c.url = (ch == '?' || ch == '#') ? HuQuery : HuPre;
                    } else if (c.url == HuPre && (ch == '?' || ch == '#')) {
                        c.url = HuQuery;
                    }
                    out.push_back(ch);
                }
                return i;
            }

            // JavaScript's expression text. A slash is read by the text
            // since the last special character (a quote, a slash, a brace,
            // <, -, #) as Go's html/template reads it: < - and # leave the
            // reading as it was, a brace makes the next slash a pattern
            size_t js(std::string_view s, size_t i) {
                while (i < s.size()) {
                    char ch = s[i];
                    if (ch == '"' || ch == '\'' || ch == '`') {
                        out.push_back(ch);
                        c.state = ch == '"' ? HsJsDq : ch == '\'' ? HsJsSq : HsJsTmpl;
                        js_tokens.clear();
                        return i + 1;
                    }
                    if (ch == '/') {
                        if (i + 1 < s.size() && s[i + 1] == '/') {
                            c.state = HsJsLineComment;
                            return i + 2;
                        }
                        if (i + 1 < s.size() && s[i + 1] == '*') {
                            c.state = HsJsBlockComment;
                            return i + 2;
                        }
                        uint8_t ctx = js_tokens.find_first_not_of(' ') == std::string::npos ? c.js : js_after(js_tokens);
                        if (ctx == HjUnknown) {
                            error = "a slash that could start a division or a regular expression";
                            return s.size();
                        }
                        out.push_back('/');
                        js_tokens.clear();
                        if (ctx == HjRegexp) {
                            c.state = HsJsRegexp;
                            return i + 1;
                        }
                        c.js = HjRegexp;
                        return i + 1;
                    }
                    if (ch == '<' && s.substr(i, 4) == "<!--") {
                        c.state = HsJsLineComment;   // an HTML-like comment of JavaScript
                        return i + 4;
                    }
                    if (ch == '-' && s.substr(i, 3) == "-->") {
                        c.state = HsJsLineComment;
                        return i + 3;
                    }
                    if (ch == '#' && i + 1 < s.size() && s[i + 1] == '!') {
                        c.state = HsJsLineComment;   // a hashbang
                        return i + 2;
                    }
                    if (ch == '<' || ch == '-' || ch == '#') {
                        out.push_back(ch);
                        js_tokens.clear();
                        ++i;
                        continue;
                    }
                    if (ch == '{' || ch == '}') {
                        if (c.braces > 0) {
                            if (ch == '{') {
                                ++c.depth[c.braces - 1];
                            } else if (c.depth[c.braces - 1] == 0) {
                                --c.braces;
                                out.push_back('}');
                                c.state = HsJsTmpl;
                                js_tokens.clear();
                                return i + 1;
                            } else {
                                --c.depth[c.braces - 1];
                            }
                        }
                        out.push_back(ch);
                        js_tokens.clear();
                        c.js = HjRegexp;
                        ++i;
                        continue;
                    }
                    out.push_back(ch);
                    if (!html_space(ch)) {
                        js_tokens.push_back(ch);
                    } else if (!js_tokens.empty() && js_tokens.back() != ' ') {
                        js_tokens.push_back(' ');
                    }
                    if (js_tokens.find_first_not_of(' ') != std::string::npos) {
                        c.js = js_after(js_tokens);
                    }
                    ++i;
                }
                return i;
            }

            size_t js_string(std::string_view s, size_t i) {
                char quote = c.state == HsJsDq ? '"' : c.state == HsJsSq ? '\'' : '`';
                while (i < s.size()) {
                    char ch = s[i];
                    if (ch == '\\' && i + 1 < s.size()) {
                        out.push_back(ch);
                        out.push_back(s[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (ch == '<' && c.element == HeScript && c.delim == HdNone
                        && element_end(s, i) != std::string_view::npos) {
                        out.append("\\x3C");
                        ++i;
                        continue;
                    }
                    if (ch == quote) {
                        out.push_back(ch);
                        c.state = HsJs;
                        c.js = HjDiv;
                        return i + 1;
                    }
                    if (quote == '`' && ch == '$' && i + 1 < s.size() && s[i + 1] == '{') {
                        out.append("${");
                        if (c.braces < 8) {
                            c.depth[c.braces++] = 0;
                        }
                        c.state = HsJs;
                        c.js = HjRegexp;
                        js_tokens.clear();
                        return i + 2;
                    }
                    if (quote != '`' && (ch == '\n' || ch == '\r')) {
                        error = "a line break inside a JavaScript string";
                        return s.size();
                    }
                    out.push_back(ch);
                    ++i;
                }
                return i;
            }

            size_t js_regexp(std::string_view s, size_t i) {
                bool in_class = false;
                while (i < s.size()) {
                    char ch = s[i];
                    if (ch == '\\' && i + 1 < s.size()) {
                        out.push_back(ch);
                        out.push_back(s[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (ch == '<' && c.element == HeScript && c.delim == HdNone
                        && element_end(s, i) != std::string_view::npos) {
                        out.append("\\x3C");
                        ++i;
                        continue;
                    }
                    if (ch == '[') {
                        in_class = true;
                    } else if (ch == ']') {
                        in_class = false;
                    } else if (ch == '/' && !in_class) {
                        out.push_back(ch);
                        c.state = HsJs;
                        c.js = HjDiv;
                        js_tokens = "x";
                        return i + 1;
                    } else if (ch == '\n' || ch == '\r') {
                        error = "a line break inside a JavaScript regular expression";
                        return s.size();
                    }
                    out.push_back(ch);
                    ++i;
                }
                if (in_class) {
                    error = "a field inside the character class of a JavaScript regular expression";
                }
                return i;
            }

            size_t css(std::string_view s, size_t i) {
                while (i < s.size()) {
                    char ch = s[i];
                    if (ch == '"' || ch == '\'') {
                        out.push_back(ch);
                        c.state = ch == '"' ? HsCssDq : HsCssSq;
                        c.url = HuNone;
                        return i + 1;
                    }
                    if (ch == '/' && i + 1 < s.size() && s[i + 1] == '*') {
                        c.state = HsCssBlockComment;
                        return i + 2;
                    }
                    if (ch == '/' && i + 1 < s.size() && s[i + 1] == '/') {
                        c.state = HsCssLineComment;
                        return i + 2;
                    }
                    if (ch == '(' && i >= 3 && (s[i - 1] | 32) == 'l' && (s[i - 2] | 32) == 'r' && (s[i - 3] | 32) == 'u'
                        && (i == 3 || !ident_char(s[i - 4]))) {
                        out.push_back('(');
                        size_t j = i + 1;
                        while (j < s.size() && css_space(s[j])) {
                            out.push_back(s[j++]);
                        }
                        if (j < s.size() && (s[j] == '"' || s[j] == '\'')) {
                            out.push_back(s[j]);
                            c.state = s[j] == '"' ? HsCssDqUrl : HsCssSqUrl;
                            c.url = HuNone;
                            return j + 1;
                        }
                        c.state = HsCssUrl;
                        c.url = HuNone;
                        return j;
                    }
                    out.push_back(ch);
                    ++i;
                }
                return i;
            }

            size_t css_string(std::string_view s, size_t i) {
                char quote = c.state == HsCssDq ? '"' : '\'';
                while (i < s.size()) {
                    char ch = s[i];
                    if (ch == '\\' && i + 1 < s.size()) {
                        out.push_back(ch);
                        out.push_back(s[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (ch == quote) {
                        out.push_back(ch);
                        c.state = HsCss;
                        c.url = HuNone;
                        return i + 1;
                    }
                    if (c.url == HuNone) {
                        c.url = (ch == '?' || ch == '#') ? HuQuery : HuPre;
                    } else if (c.url == HuPre && (ch == '?' || ch == '#')) {
                        c.url = HuQuery;
                    }
                    out.push_back(ch);
                    ++i;
                }
                return i;
            }

            size_t css_url(std::string_view s, size_t i) {
                while (i < s.size()) {
                    char ch = s[i];
                    bool close = c.state == HsCssDqUrl ? ch == '"' : c.state == HsCssSqUrl ? ch == '\'' : ch == ')';
                    if (close) {
                        out.push_back(ch);
                        c.state = HsCss;
                        c.url = HuNone;
                        return i + 1;
                    }
                    if (c.url == HuNone) {
                        c.url = (ch == '?' || ch == '#') ? HuQuery : HuPre;
                    } else if (c.url == HuPre && (ch == '?' || ch == '#')) {
                        c.url = HuQuery;
                    }
                    out.push_back(ch);
                    ++i;
                }
                return i;
            }
        };

        // The escaping of a field written where ctx is, and the context
        // after it; nullptr error when it may be written there
        inline uint32_t html_field(HtmlCtx& c, uint8_t trust, const char*& error) noexcept {
            uint8_t outer = c.delim == HdNone ? HoNone : c.delim == HdSpace ? HoUnquoted : HoQuoted;
            if (c.state == HsBeforeValue) {
                // a field that starts an unquoted value
                c.delim = HdSpace;
                switch (c.attr) {
                    case HaScript: c.state = HsJs; c.js = HjRegexp; break;
                    case HaStyle: c.state = HsCss; break;
                    case HaUrl: c.state = HsUrl; c.url = HuNone; break;
                    case HaSrcset: c.state = HsSrcset; break;
                    default: c.state = HsAttr; break;
                }
                outer = HoUnquoted;
            }
            switch (c.state) {
                case HsText:
                    return html_escape_code(HeHtml, HoNone, trust);
                case HsRcdata:
                    return html_escape_code(HeRcdata, HoNone, trust);
                case HsTag:
                case HsAttrName:
                case HsAfterName:
                    c.state = HsAttrName;
                    c.attr = HaNone;
                    return html_escape_code(HeName, HoNone, trust);
                case HsComment:
                case HsJsBlockComment:
                case HsJsLineComment:
                case HsCssBlockComment:
                case HsCssLineComment:
                    return html_escape_code(HeNothing, HoNone, trust);
                case HsAttr:
                    if (outer == HoUnquoted) {
                        return html_escape_code(HeNospace, HoNone, trust);
                    }
                    return html_escape_code(HeAttrHtml, HoNone, trust);
                case HsUrl:
                case HsCssDqUrl:
                case HsCssSqUrl:
                case HsCssUrl: {
                    uint8_t part = c.url;
                    if (part == HuUnknown) {
                        error = "a field in a URL whose part the branches before it leave unknown";
                        return 0;
                    }
                    uint8_t inner = part == HuNone ? HeUrlFilter : part == HuPre ? HeUrlNormal : HeUrlQuery;
                    if (c.state != HsUrl && part == HuQuery) {
                        inner = HeUrlQuery;
                    }
                    if (part == HuNone) {
                        c.url = HuPre;
                    }
                    return html_escape_code(inner, outer, trust);
                }
                case HsSrcset:
                    return html_escape_code(HeSrcset, outer, trust);
                case HsJs:
                    c.js = HjDiv;
                    return html_escape_code(HeJsValue, outer, trust);
                case HsJsDq:
                case HsJsSq:
                    return html_escape_code(HeJsString, outer, trust);
                case HsJsTmpl:
                    return html_escape_code(HeJsTemplate, outer, trust);
                case HsJsRegexp:
                    return html_escape_code(HeJsRegexp, outer, trust);
                case HsCss:
                    return html_escape_code(HeCssValue, outer, trust);
                case HsCssDq:
                case HsCssSq: {
                    uint8_t part = c.url;
                    if (part == HuUnknown) {
                        error = "a field in a CSS string whose URL part the branches before it leave unknown";
                        return 0;
                    }
                    if (part == HuNone) {
                        c.url = HuPre;
                    }
                    return html_escape_code(part == HuNone ? HeCssString : part == HuPre ? HeCssStringPre
                                                                                        : HeCssStringQuery,
                                            outer, trust);
                }
                default:
                    error = "a field where the HTML around it has no place for one";
                    return 0;
            }
        }

        // two contexts where branches meet: equal, or equal but for the
        // part of a URL or the reading of a slash, which become unknown
        inline bool html_join(const HtmlCtx& a, const HtmlCtx& b, HtmlCtx& out) noexcept {
            if (a == b) {
                out = a;
                return true;
            }
            HtmlCtx x = a, y = b;
            x.url = y.url = HuNone;
            x.js = y.js = HjRegexp;
            if (!(x == y)) {
                return false;
            }
            out = a;
            if (a.url != b.url) {
                out.url = HuUnknown;
            }
            if (a.js != b.js) {
                out.js = HjUnknown;
            }
            return true;
        }

        struct HtmlStencilBuilder {
            static uint8_t trust_of(const stencil& s, std::string_view source, const stencil_expr& e) noexcept {
                if (!e.pipe_size) {
                    return HtNone;
                }
                const stencil_stage& last = s._stages[e.pipe_at + e.pipe_size - 1];
                std::string_view name = source.substr(last.name_at, last.name_size);
                return name == "safe_html" ? HtHtml : name == "safe_url" ? HtUrl : name == "safe_attr" ? HtAttr
                    : name == "safe_js" ? HtJs : name == "safe_css" ? HtCss : HtNone;
            }

            static stencil_error error_at(std::string_view v, size_t offset, const char* why) noexcept {
                size_t line = 1, column = 1;
                for (size_t i = 0; i < offset && i < v.size(); ++i) {
                    if (v[i] == '\n') {
                        ++line;
                        column = 1;
                    } else {
                        ++column;
                    }
                }
                return stencil_error(offset, line, column, why);
            }

            // Reads the template's text as HTML and rewrites its program:
            // a field becomes an escaped step, a run of text the text it
            // writes; nullopt or where the HTML stops making sense
            static optional<stencil_error> rewrite(stencil& s, std::string_view source) {
                size_t n = s._steps.size();
                std::vector<HtmlCtx> in(n + 1);
                std::vector<bool> known(n + 1, false);
                std::vector<std::vector<stencil_step>> made(n);
                std::string extra;
                size_t last_offset = 0;
                known[0] = true;
                auto flow = [&](size_t to, const HtmlCtx& ctx, size_t at) -> const char* {
                    if (!known[to]) {
                        in[to] = ctx;
                        known[to] = true;
                        return nullptr;
                    }
                    HtmlCtx joined;
                    if (!html_join(in[to], ctx, joined)) {
                        (void)at;
                        return "branches end in different HTML contexts";
                    }
                    in[to] = joined;
                    return nullptr;
                };
                for (size_t pc = 0; pc < n; ++pc) {
                    const stencil_step& st = s._steps[pc];
                    if (!known[pc]) {
                        // reached by nothing: written as it is, in text
                        in[pc] = HtmlCtx();
                        known[pc] = true;
                    }
                    HtmlCtx c = in[pc];
                    const char* why = nullptr;
                    switch (st.op) {
                        case stencil_op::text: {
                            last_offset = st.a + st.b;
                            HtmlScanner scan;
                            scan.c = c;
                            scan.run(source.substr(st.a, st.b));
                            if (scan.error) {
                                return error_at(source, st.a, scan.error);
                            }
                            c = scan.c;
                            std::string_view orig = source.substr(st.a, st.b);
                            if (scan.out == orig) {
                                made[pc].push_back(st);
                            } else if (!scan.out.empty()) {
                                stencil_step x;
                                x.op = stencil_op::extra;
                                x.a = uint32_t(extra.size());
                                x.b = uint32_t(scan.out.size());
                                extra.append(scan.out);
                                made[pc].push_back(x);
                            }
                            why = flow(pc + 1, c, st.a);
                            break;
                        }
                        case stencil_op::write: {
                            const char* err = nullptr;
                            uint32_t code = html_field(c, trust_of(s, source, s._exprs[st.a]), err);
                            if (err) {
                                return error_at(source, last_offset, err);
                            }
                            stencil_step x;
                            x.op = stencil_op::escaped;
                            x.a = st.a;
                            x.b = code;
                            made[pc].push_back(x);
                            why = flow(pc + 1, c, last_offset);
                            break;
                        }
                        case stencil_op::branch:
                        case stencil_op::enter:
                        case stencil_op::loop:
                            made[pc].push_back(st);
                            why = flow(pc + 1, c, last_offset);
                            if (!why) {
                                why = flow(st.b, c, last_offset);
                            }
                            break;
                        case stencil_op::jump:
                            made[pc].push_back(st);
                            why = flow(st.b, c, last_offset);
                            break;
                        case stencil_op::repeat:
                            made[pc].push_back(st);
                            // the loop's body begins again where it began
                            if (!(in[st.b] == c)) {
                                HtmlCtx j;
                                if (!html_join(in[st.b], c, j) || !(j == in[st.b])) {
                                    return error_at(source, last_offset,
                                                    "a range whose body ends in another HTML context than it begins");
                                }
                            }
                            why = flow(pc + 1, c, last_offset);
                            break;
                        default:
                            made[pc].push_back(st);
                            why = flow(pc + 1, c, last_offset);
                            break;
                    }
                    if (why) {
                        return error_at(source, last_offset, why);
                    }
                }
                const HtmlCtx& end = in[n];
                if (known[n] && !(end.state == HsText && end.element == HeNone)) {
                    if (end.state != HsText) {
                        return error_at(source, source.size(), "the template ends inside a tag, an attribute, "
                                                                     "a script, a style or a comment");
                    }
                }
                // the new program, its targets moved
                std::vector<uint32_t> at(n + 1);
                uint32_t k = 0;
                for (size_t pc = 0; pc < n; ++pc) {
                    at[pc] = k;
                    k += uint32_t(made[pc].size());
                }
                at[n] = k;
                vector<stencil_step> steps;
                steps.reserve(k);
                for (size_t pc = 0; pc < n; ++pc) {
                    for (auto x : made[pc]) {
                        switch (x.op) {
                            case stencil_op::branch: case stencil_op::jump: case stencil_op::enter:
                            case stencil_op::loop: case stencil_op::repeat:
                                x.b = at[x.b];
                                break;
                            default: break;
                        }
                        steps.push_back(x);
                    }
                }
                s._steps = std::move(steps);
                s._extra = string(std::string_view(extra));
                s._escaper = &html_write;
                return nullopt;
            }

            // The template of text read where it lies (the caller's memory,
            // which a fuzzer's tools watch), keep the library's string of
            // the same text the template holds
            template<class H>
            static expected<H, stencil_error> make(std::string_view text, const string& keep,
                                                   const stencil_functions& functions) {
                StencilParser parser{keep, text, functions, stencil()};
                parser.out._source = keep;
                if (!parser.run()) {
                    return unexpected<stencil_error>(parser.why);
                }
                parser.finish();
                if (auto e = rewrite(parser.out, text)) {
                    return unexpected<stencil_error>(*e);
                }
                H h;
                h._stencil = std::move(parser.out);
                return h;
            }
        };

        inline stencil_functions html_functions(const stencil_functions& base);

        // the six functions every template has and the five safe_ ones
        inline const stencil_functions& html_builtin_functions() {
            static root_ptr<stencil_functions> table =
                make_tracked<stencil_functions>(html_functions(stencil_functions::builtin()));
            return *table;
        }

        inline stencil_functions html_functions(const stencil_functions& base) {
            stencil_functions f = base;
            auto same = [](const value& v, slice<const value>) { return v; };
            for (const char* name : {"safe_html", "safe_url", "safe_attr", "safe_js", "safe_css"}) {
                f.add(string(name), same);
            }
            return f;
        }
    }

    class html_stencil {
    public:
        html_stencil() = default;

        // A template read from outside the program: the template, or where
        // and why it is not one (as stencil::parse), or where the HTML
        // around a field gives it no safe place
        static expected<html_stencil, stencil_error> parse(const string& source) noexcept {
            static const stencil_functions& table = detail::html_builtin_functions();
            return _parse(source, table);
        }

        static expected<html_stencil, stencil_error> parse(const string& source, const stencil_functions& functions) {
            return _parse(source, detail::html_functions(functions));
        }

        // The template a literal of the program spells: parse's value, or
        // bad_expected_access<stencil_error> (DESIGN 234)
        explicit html_stencil(const string& source)
        : html_stencil(parse(source).value()) {
        }

        explicit html_stencil(const string& source, const stencil_functions& functions)
        : html_stencil(parse(source, functions).value()) {
        }

        static bool parses(const string& source) noexcept {
            return parse(source).has_value();
        }

        string render(const value& data) const {
            return _stencil.render(data);
        }

        size_t render_to(const slice<char>& buffer, const value& data) const {
            return _stencil.render_to(buffer, data);
        }

        const string& source() const noexcept {
            return _stencil.source();
        }

    private:
        stencil _stencil;

        friend struct detail::HtmlStencilBuilder;

        static expected<html_stencil, stencil_error> _parse(const string& source, const stencil_functions& functions) {
            return detail::HtmlStencilBuilder::make<html_stencil>(source.view(), source, functions);
        }
    };
}
