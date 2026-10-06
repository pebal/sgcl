//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// html_stencil on any input. The first byte picks the path; the rest is the
// input. What must hold:
//   - a template of any text is refused or renders, twice alike, render_to
//     telling render's size, valid UTF-8 from valid UTF-8, with any data;
//   - a value of any text, in each of the fixed contexts below, cannot leave
//     its context: in text and in an attribute no < > or quote of its own, a
//     URL never begins with a scheme other than http, https or mailto, a
//     script string never closes, a script never ends early.
// The template is read from libFuzzer's own buffer, never from a copy on the
// managed heap, where ASan does not see a read past the end.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/html_stencil_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/txt/txt.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    txt::value data_of(std::string_view v) {
        string s(v);
        return txt::object{{"x", s}, {"n", 42}, {"b", true}, {"l", txt::list{s, 1.5}}, {"o", txt::object{{"k", s}}}};
    }

    void template_path(std::string_view in, uint8_t mode) {
        // parsed where libFuzzer's tools watch: from the input itself, of
        // exactly its size, the library's string kept only for the template
        auto t = txt::detail::HtmlStencilBuilder::make<txt::html_stencil>(in, string(in),
                                                                          txt::detail::html_builtin_functions());
        if (!t) {
            check(!t.error().message().empty());
            return;
        }
        txt::value d = data_of(mode & 1 ? std::string_view("<script>\"'&") : in.substr(0, 16));
        string a = t->render(d);
        string b = t->render(d);
        check(a == b);
        char room[64];
        check(t->render_to(slice<char>(room, sizeof room), d) == a.size());
        check(!utf8::valid(in) || utf8::valid(a.view()));
    }

    bool lower_starts(std::string_view s, std::string_view prefix) {
        if (s.size() < prefix.size()) {
            return false;
        }
        for (size_t i = 0; i < prefix.size(); ++i) {
            char c = s[i];
            if (c >= 'A' && c <= 'Z') {
                c = char(c + 32);
            }
            if (c != prefix[i]) {
                return false;
            }
        }
        return true;
    }

    // what a field wrote between a context's prefix and suffix
    std::string_view between(const string& page, std::string_view pre, std::string_view post) {
        std::string_view p = page.view();
        check(p.size() >= pre.size() + post.size());
        check(p.substr(0, pre.size()) == pre);
        check(p.substr(p.size() - post.size()) == post);
        return p.substr(pre.size(), p.size() - pre.size() - post.size());
    }

    void context_path(std::string_view in, uint8_t mode) {
        struct Ctx {
            const char* pre;
            const char* post;
            const char* forbidden;   // bytes the field may not write
            int kind;                // 0 plain, 1 URL start, 2 script string
        };
        static const Ctx contexts[] = {
            {"<p>", "</p>", "<>\"'", 0},
            {"<a title=\"", "\">", "<>\"'", 0},
            {"<a title='", "'>", "<>\"'", 0},
            {"<a title=", ">", "<>\"' \t\n\f\r=`", 0},
            {"<a href=\"", "\">", "<>\"' ", 1},
            {"<a href=\"/x?q=", "\">", "<>\"' &?#", 0},
            {"<script>var s = \"", "\";</script>", "\"'<>\n\r", 2},
            {"<script>var s = ", ";</script>", "<>\n\r", 0},
            {"<style>p { color: ", " }</style>", "<>\"'(){};", 0},
            {"<style>p { content: \"", "\" }</style>", "<>\"'\n", 0},
            {"<a onclick=\"f(", ")\">", "<>\"'", 0},
            {"<textarea>", "</textarea>", "<>", 0},
        };
        const Ctx& c = contexts[(mode >> 1) % (sizeof contexts / sizeof contexts[0])];
        std::string source = std::string(c.pre) + "{{ x }}" + c.post;
        txt::html_stencil t{string(std::string_view(source))};
        string page = t.render(data_of(in));
        std::string_view field = between(page, c.pre, c.post);
        for (char ch : field) {
            for (const char* f = c.forbidden; *f; ++f) {
                check(ch != *f);
            }
        }
        if (c.kind == 1) {
            // no scheme but http, https and mailto
            size_t colon = field.find(':');
            if (colon != std::string_view::npos && field.substr(0, colon).find('/') == std::string_view::npos) {
                check(lower_starts(field, "http:") || lower_starts(field, "https:") || lower_starts(field, "mailto:"));
            }
        }
        if (c.kind == 2) {
            // a backslash escapes, so the string ends only at the suffix's quote
            for (size_t i = 0; i < field.size(); ++i) {
                if (field[i] == '\\') {
                    ++i;
                }
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    if (mode & 0x80) {
        context_path(rest, mode);
    } else {
        template_path(rest, mode);
    }
    return 0;
}
