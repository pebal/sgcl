//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Diff, patch and merge on any input. The first byte picks the options; the
// rest splits at its first two NUL bytes into up to three texts, each copied
// into a buffer of exactly its size (never a managed copy, where ASan does not
// see a read past the end). What must hold:
//   - the edit script of two texts (by lines, words or code points, Myers or
//     patience) covers both in order and rebuilds the second from the first;
//   - the unified diff of two texts applied to the first gives the second,
//     and applied in reverse to the second gives the first;
//   - the second text read as a patch applies to the first or fails cleanly;
//   - the merge of three texts: ours when both sides are equal, theirs when
//     ours is the base, never a conflict in either case.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/diff_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/txt/txt.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    namespace d = txt::detail::diff;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Exact {
        char* p;
        size_t n;

        explicit Exact(std::string_view s)
        : p(static_cast<char*>(std::malloc(s.size() ? s.size() : 1)))
        , n(s.size()) {
            std::copy(s.begin(), s.end(), p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(p);
        }

        std::string_view view() const {
            return std::string_view(p, n);
        }
    };

    void script_path(std::string_view a, std::string_view b, uint8_t mode) {
        txt::diff_options o;
        o.algorithm = mode & 1 ? txt::diff_algorithm::patience : txt::diff_algorithm::myers;
        o.ignore_whitespace = (mode & 6) == 6;
        std::vector<d::Unit> ua, ub;
        switch ((mode >> 1) % 3) {
            case 0:
                ua = d::lines_of(a);
                ub = d::lines_of(b);
                break;
            case 1:
                ua = d::words_of(a);
                ub = d::words_of(b);
                break;
            default:
                ua = d::chars_of(a);
                ub = d::chars_of(b);
        }
        d::Script s = d::make_script(a, b, std::move(ua), std::move(ub), o);
        std::vector<txt::diff_edit> e = d::edits(s.ua, s.ub, s.ca, s.cb, a.size(), b.size());
        std::string rebuilt;
        size_t pa = 0, pb = 0;
        for (const auto& x : e) {
            check(x.old_begin == pa && x.new_begin == pb && x.old_begin <= x.old_end && x.new_begin <= x.new_end);
            check(x.old_end <= a.size() && x.new_end <= b.size());
            if (x.kind == txt::diff_kind::equal) {
                if (!o.ignore_whitespace) {
                    check(a.substr(x.old_begin, x.old_end - x.old_begin) == b.substr(x.new_begin, x.new_end - x.new_begin));
                }
                rebuilt += b.substr(x.new_begin, x.new_end - x.new_begin);
            } else if (x.kind == txt::diff_kind::insert) {
                check(x.old_begin == x.old_end);
                rebuilt += b.substr(x.new_begin, x.new_end - x.new_begin);
            } else {
                check(x.new_begin == x.new_end);
            }
            pa = x.old_end;
            pb = x.new_end;
        }
        check(pa == a.size() && pb == b.size());
        check(rebuilt == b);
    }

    std::string apply(std::string_view text, std::string_view patch, const txt::patch_options& o, bool& ok) {
        std::vector<d::Hunk> hunks;
        ok = false;
        if (d::parse_patch(patch, hunks)) {
            return std::string();
        }
        optional<txt::patch_error> err;
        std::string out = d::apply(text, hunks, o, err);
        ok = !err;
        return out;
    }

    void patch_path(std::string_view a, std::string_view b, uint8_t mode) {
        txt::unified_options u;
        u.context = mode % 5;
        u.algorithm = mode & 8 ? txt::diff_algorithm::patience : txt::diff_algorithm::myers;
        std::string p = d::unified(a, b, u);
        if (a == b) {
            check(p.empty());
        } else {
            Exact patch(p);
            bool ok;
            std::string got = apply(a, patch.view(), {.fuzz = 0}, ok);
            check(ok && got == b);
            got = apply(b, patch.view(), {.fuzz = 0, .reverse = true}, ok);
            check(ok && got == a);
        }
        // the second text as a patch of the first
        bool ok;
        apply(a, b, {.fuzz = size_t((mode >> 5) & 3), .reverse = (mode & 16) != 0}, ok);
    }

    void merge_path(std::string_view base, std::string_view ours, std::string_view theirs, uint8_t mode) {
        txt::merge_options o;
        o.diff3 = mode & 1;
        size_t conflicts = 0;
        std::string m = d::merge(base, ours, theirs, o, conflicts);
        if (ours == theirs || base == ours) {
            check(conflicts == 0 && m == theirs);
        } else if (base == theirs) {
            check(conflicts == 0 && m == ours);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 8192) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    size_t z1 = rest.find('\0');
    std::string_view p0 = rest.substr(0, z1), p1, p2;
    if (z1 != std::string_view::npos) {
        std::string_view r = rest.substr(z1 + 1);
        size_t z2 = r.find('\0');
        p1 = r.substr(0, z2);
        if (z2 != std::string_view::npos) {
            p2 = r.substr(z2 + 1);
        }
    }
    Exact a(p0), b(p1), c(p2);
    switch (mode >> 6) {
        case 0:
            script_path(a.view(), b.view(), mode);
            break;
        case 1:
        case 2:
            patch_path(a.view(), b.view(), mode);
            break;
        default:
            merge_path(a.view(), b.view(), c.view(), mode);
    }
    return 0;
}
