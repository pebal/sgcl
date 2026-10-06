//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "error.h"
#include "fs.h"
#include "path.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <algorithm>
#include <dirent.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <vector>

namespace sgcl::io {
    // Glob patterns as the shell and Python's glob have them, beyond Go's
    // (io::path::match and io::path::glob stay Go's filepath.Match and
    // Glob): `**` as a whole component, zero or more directories; `{a,b}`
    // braces, nested, expanded when the pattern is compiled; `*`, `?`,
    // `[...]` (`!` or `^` negating, ranges of code points) and `\`
    // escaping, per component as path::match has them; a trailing `/` for
    // directories only. A compiled pattern (glob_pattern) matches a path
    // by itself, apart from any walk; io::glob walks the file system with
    // it. What a walk yields is Python's glob(recursive=True), its oracle:
    // `a/**` yields `a/` and everything under it, a pattern ending in `/`
    // the directories with their slash; wildcards pass over names that
    // begin with "." unless the pattern's component begins with one
    // (Python's hidden files; glob_options::hidden takes them in); `**`
    // enters no link to a directory (no loops; Python follows them); the
    // result is sorted by its bytes, each path once.

    // How a pattern matches beyond its text
    struct glob_options {
        bool hidden = false;   // wildcards match names beginning with "." too (Python's include_hidden)
    };

    class glob_pattern;

    namespace detail {
        // One alternative of a pattern, its braces expanded: the components
        // between the separators ("**" as it is), rooted at "/" or not,
        // for directories only (a trailing "/") or not
        struct GlobAlternative {
            vector<string> parts;
            bool rooted = false;
            bool dir_only = false;
            bool star = false;   // a `**` among the parts
        };

        class GlobState final {
        public:
            string text;
            bool hidden = false;
            bool literal = true;
            vector<GlobAlternative> alternatives;
        };

        inline constexpr size_t GlobMaxAlternatives = 1024;

        // Where a class that begins at p (a '[') ends: the index of its ']',
        // npos when it does not close (path::detail::valid_pattern's rules)
        inline size_t glob_class_end(std::string_view s, size_t p) noexcept {
            size_t q = p + 1;
            if (q < s.size() && (s[q] == '!' || s[q] == '^')) {
                ++q;
            }
            bool any = false;
            while (q < s.size()) {
                if (s[q] == ']' && any) {
                    return q;
                }
                if (s[q] == '\\') {
                    ++q;
                }
                ++q;
                any = true;
            }
            return std::string_view::npos;
        }

        // The pattern's braces expanded into `out`, outermost first: the
        // first '{' that closes and holds a comma at its level (one that does
        // not is a '{' as it is, as in the shell), each of its alternatives
        // put in its place and the result expanded again; false past
        // GlobMaxAlternatives
        inline bool glob_expand(std::string_view s, std::vector<std::string>& out) {
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '\\') {
                    ++i;
                    continue;
                }
                if (s[i] == '[') {
                    size_t end = glob_class_end(s, i);
                    if (end != std::string_view::npos) {
                        i = end;
                    }
                    continue;
                }
                if (s[i] != '{') {
                    continue;
                }
                // the '}' that closes it, and the commas at its level
                std::vector<size_t> commas;
                int depth = 0;
                size_t close = std::string_view::npos;
                for (size_t j = i + 1; j < s.size() && close == std::string_view::npos; ++j) {
                    if (s[j] == '\\') {
                        ++j;
                    } else if (s[j] == '[') {
                        size_t end = glob_class_end(s, j);
                        if (end != std::string_view::npos) {
                            j = end;
                        }
                    } else if (s[j] == '{') {
                        ++depth;
                    } else if (s[j] == '}') {
                        if (depth == 0) {
                            close = j;
                        } else {
                            --depth;
                        }
                    } else if (s[j] == ',' && depth == 0) {
                        commas.push_back(j);
                    }
                }
                if (close == std::string_view::npos || commas.empty()) {
                    continue;   // a '{' as it is
                }
                std::string_view before = s.substr(0, i);
                std::string_view after = s.substr(close + 1);
                size_t from = i + 1;
                commas.push_back(close);
                for (size_t c : commas) {
                    std::string one;
                    one.reserve(before.size() + (c - from) + after.size());
                    one.append(before);
                    one.append(s.substr(from, c - from));
                    one.append(after);
                    if (!glob_expand(one, out)) {
                        return false;
                    }
                    from = c + 1;
                }
                return true;
            }
            if (out.size() >= GlobMaxAlternatives) {
                return false;
            }
            out.emplace_back(s);
            return true;
        }

        SGCL_INLINE_HOT bool glob_has_meta(std::string_view s) noexcept {
            return s.find_first_of("*?[\\") != std::string_view::npos;
        }

        // A name the component may stand for, by the hidden rule: a name
        // beginning with "." only when the component begins with one too
        // (or hidden is set), "." and ".." never by a wildcard
        SGCL_INLINE_HOT bool glob_visible(std::string_view part, std::string_view name, bool hidden) noexcept {
            if (name.empty() || name[0] != '.') {
                return true;
            }
            if (name == "." || name == "..") {
                return false;
            }
            return hidden || (!part.empty() && part[0] == '.');
        }

        // One component of the pattern against one of a name
        SGCL_INLINE_HOT bool glob_component(std::string_view part, std::string_view name, bool hidden) noexcept {
            if (!glob_has_meta(part)) {
                return part == name;
            }
            return glob_visible(part, name, hidden) && path::detail::match_element(part, name).value_or(false);
        }

        // The components of a path against an alternative's from (p, n), `**`
        // over any number of them (none hidden, unless hidden is set); `seen`
        // the pairs found not to match, so that the search is quadratic at
        // worst whatever the number of `**`
        // A `**` that ends the pattern and stands for no component matches
        // the directory itself, which the walk writes with its slash ("a/"
        // for "a/**", as Python's glob and its translate have it): the
        // path's trailing "/" is asked for then
        inline bool glob_match_from(const vector<string>& parts, const std::vector<std::string_view>& names, size_t p, size_t n, bool hidden, bool trailing, std::vector<uint8_t>& seen) noexcept {
            const size_t width = names.size() + 1;
            while (p < parts.size()) {
                std::string_view part = parts[p].view();
                if (part == "**") {
                    if (seen[p * width + n]) {
                        return false;
                    }
                    for (size_t k = n;; ++k) {
                        if ((k > n || trailing || p + 1 < parts.size()) && glob_match_from(parts, names, p + 1, k, hidden, trailing, seen)) {
                            return true;
                        }
                        if (k == names.size() || !glob_visible("", names[k], hidden)) {
                            break;
                        }
                    }
                    seen[p * width + n] = 1;
                    return false;
                }
                if (n == names.size() || !glob_component(part, names[n], hidden)) {
                    return false;
                }
                ++p;
                ++n;
            }
            return n == names.size();
        }

        // Without `**`: the components one by one against the path's, as they
        // come, nothing allocated
        inline bool glob_match_plain(const GlobAlternative& a, std::string_view path, size_t at, bool hidden) noexcept {
            size_t p = 0;
            while (at < path.size()) {
                size_t end = path.find('/', at);
                if (end == std::string_view::npos) {
                    end = path.size();
                }
                if (end > at) {
                    if (p == a.parts.size() || !glob_component(a.parts[p].view(), path.substr(at, end - at), hidden)) {
                        return false;
                    }
                    ++p;
                }
                at = end + 1;
            }
            return p == a.parts.size();
        }

        inline bool glob_match(const GlobState& s, std::string_view path) noexcept {
            const bool rooted = !path.empty() && path[0] == '/';
            const bool trailing = path.size() > 1 && path.back() == '/';
            const size_t start = rooted ? 1 : 0;
            std::vector<std::string_view> names;   // split once, for the alternatives with `**`
            std::vector<uint8_t> seen;
            for (const GlobAlternative& a : s.alternatives) {
                // a path written as a directory ("a/b/") is matched by a
                // pattern ending in "/" or in `**`, and only by those
                // (Python's translate); one ending in "/" asks for it
                if (a.rooted != rooted || (a.dir_only && !trailing)
                    || (trailing && !a.dir_only && (a.parts.empty() || a.parts.back().view() != "**"))) {
                    continue;
                }
                if (!a.star) {
                    if (glob_match_plain(a, path, start, s.hidden)) {
                        return true;
                    }
                    continue;
                }
                if (names.empty()) {
                    for (size_t at = start; at < path.size();) {
                        size_t end = path.find('/', at);
                        if (end == std::string_view::npos) {
                            end = path.size();
                        }
                        if (end > at) {
                            names.push_back(path.substr(at, end - at));
                        }
                        at = end + 1;
                    }
                }
                seen.assign((a.parts.size() + 1) * (names.size() + 1), 0);
                if (glob_match_from(a.parts, names, 0, 0, s.hidden, trailing, seen)) {
                    return true;
                }
            }
            return false;
        }

        // The pattern compiled: braces expanded, each alternative split at
        // its separators and every component checked; the error's reason
        // for a malformed one
        inline expected<tracked_ptr<GlobState>, error> glob_compile(const string& text, const glob_options& options) {
            std::vector<std::string> expanded;
            if (!glob_expand(text.view(), expanded)) {
                return detail::fail(error(errc::invalid_pattern, "glob", text));
            }
            tracked_ptr<GlobState> s = make_tracked<GlobState>();
            s->text = text;
            s->hidden = options.hidden;
            s->literal = expanded.size() == 1;
            for (const std::string& one : expanded) {
                GlobAlternative a;
                std::string_view v(one);
                a.rooted = !v.empty() && v[0] == '/';
                a.dir_only = v.size() > 1 && v.back() == '/';
                size_t at = a.rooted ? 1 : 0;
                while (at < v.size()) {
                    size_t end = v.find('/', at);
                    if (end == std::string_view::npos) {
                        end = v.size();
                    }
                    if (end > at) {
                        std::string_view part = v.substr(at, end - at);
                        if (!path::detail::valid_pattern(part)) {
                            return detail::fail(error(errc::invalid_pattern, "glob", text));
                        }
                        s->literal = s->literal && !glob_has_meta(part);
                        a.star = a.star || part == "**";
                        a.parts.push_back(string(part));
                    }
                    at = end + 1;
                }
                if (a.parts.empty() && !a.rooted) {
                    continue;   // an empty alternative names nothing
                }
                s->alternatives.push_back(std::move(a));
            }
            return s;
        }

        struct GlobAccess;
    }

    // A glob pattern, compiled: its braces expanded, its components
    // checked, matched against a path by itself (match) or walked over the
    // file system (io::glob). A handle of one word, immutable, its copies
    // the same pattern.
    class glob_pattern final {
    public:
        // The pattern a literal in the program spells, compiled: what parse
        // gives, or bad_expected_access<io::error> with parse's error for a
        // malformed one (DESIGN 234: a pattern from outside is parsed, one
        // the program wrote is constructed)
        explicit glob_pattern(const string& pattern, const glob_options& options = {})
        : glob_pattern(parse(pattern, options).value()) {
        }

        // The pattern compiled: its braces expanded, its components checked;
        // errc::invalid_pattern for a malformed one (a class that does not
        // close or holds a range backwards, a '\' at the end, more than 1024
        // alternatives of its braces)
        static expected<glob_pattern, error> parse(const string& pattern, const glob_options& options = {}) noexcept {
            auto s = detail::glob_compile(pattern, options);
            if (!s) {
                return detail::fail(s);
            }
            return glob_pattern(std::move(*s));
        }

        // Whether the path matches: its components, separated by "/", against
        // the pattern's, `**` over any number of them, a wildcard over no
        // hidden name (unless options.hidden); a path ending in "/" is a
        // directory, matched only by a pattern ending in "/" (which asks for
        // one) or in `**` ("a/" by "a/**", the directory itself, as the walk
        // writes it); the path as it is written, not cleaned. Python's
        // glob.translate is its oracle. What io::glob tests each path with
        SGCL_INLINE_HOT bool match(const string& path) const noexcept {
            return detail::glob_match(*_state, path.view());
        }

        // The pattern as it was given
        SGCL_INLINE_HOT const string& text() const noexcept {
            return _state->text;
        }

        // Whether it names one path only: no wildcard, no braces
        SGCL_INLINE_HOT bool is_literal() const noexcept {
            return _state->literal;
        }

    private:
        friend struct detail::GlobAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit glob_pattern(tracked_ptr<detail::GlobState> state) noexcept
        : _state(std::move(state)) {
        }

        SGCL_INLINE_HOT glob_pattern(sgcl::detail::FromWord, const tracked_ptr<detail::GlobState>& w) noexcept
        : _state(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::GlobState>& _handle_word() noexcept {
            return _state;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::GlobState>& _handle_word() const noexcept {
            return _state;
        }

        tracked_ptr<detail::GlobState> _state;
    };

    namespace detail {
        struct GlobAccess {
            SGCL_INLINE_HOT static const GlobState& state(const glob_pattern& p) noexcept {
                return *p._state;
            }
        };

        // The entries of a directory as readdir gives them: the name (a view
        // into the stream's buffer, valid during the call) and whether it is
        // a directory, a link or something else; "." and ".." left out. A
        // directory that cannot be opened has none (Go and Python skip it)
        enum class GlobType : uint8_t { other, directory, link, unknown };

        template<class F>
        void glob_each(const std::string& dir, F&& f) {
            DIR* d = ::opendir(dir.empty() ? "." : dir.c_str());
            if (!d) {
                return;
            }
            while (struct ::dirent* e = ::readdir(d)) {
                std::string_view name(e->d_name);
                if (name == "." || name == "..") {
                    continue;
                }
                GlobType t = e->d_type == DT_DIR ? GlobType::directory : e->d_type == DT_LNK ? GlobType::link
                           : e->d_type == DT_UNKNOWN ? GlobType::unknown : GlobType::other;
                f(name, t);
            }
            ::closedir(d);
        }

        // The walk of one alternative: `dir` the path so far as the result
        // writes it ("" the working directory, "/" the root), `p` the next
        // component; matches added to `out`. The directories are listed
        // with readdir, the names matched where they lie, a path made only
        // for what matches or is walked into
        class GlobWalk {
        public:
            GlobWalk(const GlobAlternative& a, bool hidden, std::vector<std::string>& out) noexcept
            : _a(a), _hidden(hidden), _out(out) {
            }

            void run() {
                _step(_a.rooted ? std::string("/") : std::string(), 0);
            }

        private:
            static std::string _join(const std::string& dir, std::string_view name) {
                std::string p;
                p.reserve(dir.size() + 1 + name.size());
                p = dir;
                if (!p.empty() && p.back() != '/') {
                    p += '/';
                }
                p.append(name);
                return p;
            }

            // A directory, a link to one (stat), or for a file system that
            // does not say, whatever stat says
            static bool _is_dir(const std::string& path, GlobType t) noexcept {
                if (t == GlobType::directory) {
                    return true;
                }
                if (t == GlobType::link || t == GlobType::unknown) {
                    struct ::stat st;
                    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
                }
                return false;
            }

            // A directory itself, a link not followed (`**` walks into none)
            static bool _is_real_dir(const std::string& path, GlobType t) noexcept {
                if (t == GlobType::unknown) {
                    struct ::stat st;
                    return ::lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
                }
                return t == GlobType::directory;
            }

            void _yield(std::string path, bool is_dir) {
                if (_a.dir_only) {
                    if (is_dir) {
                        if (path.back() != '/') {
                            path += '/';
                        }
                        _out.push_back(std::move(path));
                    }
                } else {
                    _out.push_back(std::move(path));
                }
            }

            // Every directory under `dir` ("**"), none hidden unless asked,
            // no link followed; files too when `files`, each with whether it
            // is a directory (a link to one counted, for a trailing "/")
            void _descendants(const std::string& dir, std::vector<std::pair<std::string, bool>>& found, bool files) {
                glob_each(dir, [&](std::string_view name, GlobType t) {
                    if (!glob_visible("", name, _hidden)) {
                        return;
                    }
                    std::string path = _join(dir, name);
                    const bool real = _is_real_dir(path, t);
                    if (real) {
                        found.push_back({path, true});
                        _descendants(path, found, files);
                    } else if (files) {
                        const bool is_dir = _a.dir_only && _is_dir(path, t);
                        found.push_back({std::move(path), is_dir});
                    }
                });
            }

            void _step(const std::string& dir, size_t p) {
                if (p == _a.parts.size()) {   // a rooted pattern of no component: "/"
                    _out.push_back(dir);
                    return;
                }
                const bool last = p + 1 == _a.parts.size();
                std::string_view part = _a.parts[p].view();
                if (part == "**") {
                    std::vector<std::pair<std::string, bool>> under;
                    _descendants(dir, under, last && !_a.dir_only);
                    if (last) {
                        if (!dir.empty() && io::is_directory(string(dir))) {
                            _out.push_back(dir.back() == '/' ? dir : dir + '/');   // the empty match: the directory itself, with its slash
                        }
                        for (auto& [path, is_dir] : under) {
                            _yield(std::move(path), is_dir);
                        }
                        return;
                    }
                    _step(dir, p + 1);
                    for (auto& [path, is_dir] : under) {
                        if (is_dir) {
                            _step(path, p + 1);
                        }
                    }
                    return;
                }
                if (!glob_has_meta(part)) {
                    std::string path = _join(dir, part);
                    if (!last) {
                        _step(path, p + 1);
                        return;
                    }
                    struct ::stat st;
                    if (::lstat(path.c_str(), &st) != 0) {
                        return;
                    }
                    const bool is_dir = S_ISDIR(st.st_mode) || (S_ISLNK(st.st_mode) && io::is_directory(string(path)));
                    _yield(std::move(path), is_dir);
                    return;
                }
                std::vector<std::pair<std::string, GlobType>> next;   // the directories to walk into, after the listing is closed
                glob_each(dir, [&](std::string_view name, GlobType t) {
                    if (!glob_component(part, name, _hidden)) {
                        return;
                    }
                    if (last) {
                        std::string path = _join(dir, name);
                        const bool is_dir = _a.dir_only && _is_dir(path, t);
                        _yield(std::move(path), is_dir);
                    } else if (t != GlobType::other) {
                        next.push_back({_join(dir, name), t});
                    }
                });
                for (auto& [path, t] : next) {
                    if (_is_dir(path, t)) {
                        _step(path, p + 1);
                    }
                }
            }

            const GlobAlternative& _a;
            bool _hidden;
            std::vector<std::string>& _out;
        };

        inline vector<string> glob_walk(const GlobState& s) {
            std::vector<std::string> found;
            for (const GlobAlternative& a : s.alternatives) {
                GlobWalk(a, s.hidden, found).run();
            }
            std::sort(found.begin(), found.end());
            found.erase(std::unique(found.begin(), found.end()), found.end());
            vector<string> out;
            out.reserve(found.size());
            for (auto& f : found) {
                out.push_back(string(f));
            }
            return out;
        }
    }

    // The paths that match the pattern, from the working directory (or the
    // root, for a pattern that begins with "/"), as glob_pattern describes
    // them: sorted by their bytes, each once; a directory that cannot be
    // read is skipped, as Go's and Python's glob skip it.
    // errc::invalid_pattern for a malformed pattern
    inline expected<vector<string>, error> glob(const string& pattern, const glob_options& options = {}) noexcept {
        auto s = detail::glob_compile(pattern, options);
        if (!s) {
            return detail::fail(s);
        }
        return detail::glob_walk(**s);
    }

    inline expected<vector<string>, error> glob(const glob_pattern& pattern) noexcept {
        return detail::glob_walk(detail::GlobAccess::state(pattern));
    }

    // `glob(...)` on this thread, `co_await async_glob(...)` in a task, on
    // the blocking pool (the directories are read as files are)
    SGCL_INLINE_HOT async::task<expected<vector<string>, error>> async_glob(const string& pattern, const glob_options& options = {}) noexcept {
        return detail::on_pool([pattern, options] { return glob(pattern, options); });
    }
}
