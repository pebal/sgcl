//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "keys.h"
#include "../../core/detail/handle_word.h"
#include "../../io/file.h"
#include "../../time/datetime.h"

#include <ctime>
#include <string>
#include <string_view>

// OpenSSH's authorized_keys file (sshd(8), "AUTHORIZED_KEYS FILE FORMAT"):
// the keys a server lets a user in with, a line "[options] type base64
// [comment]", the options a comma's list of names and name="value" pairs
// (a value quoted, \" inside it). What a server does with the options is
// its own (this module's server runs no command of its own); cert-authority
// marks a key that signs users' certificates, principals="…" the names a
// certificate must hold for it, expiry-time="YYYYMMDD[HHMM[SS]]" the end
// of the line's validity.
namespace sgcl::net::ssh {
    namespace detail {
        // The options in front of a key, read: false for a list that does
        // not end before the key (an open quote)
        inline bool read_key_options(std::string_view& line, vector<pair<string, string>>& out) {
            size_t i = 0;
            for (;;) {
                size_t b = i;
                while (i < line.size() && line[i] != '=' && line[i] != ',' && line[i] != ' ' && line[i] != '\t') {
                    ++i;
                }
                std::string_view name = line.substr(b, i - b);
                std::string value;
                if (i < line.size() && line[i] == '=') {
                    ++i;
                    if (i >= line.size() || line[i] != '"') {
                        return false;
                    }
                    ++i;
                    bool closed = false;
                    while (i < line.size()) {
                        char c = line[i++];
                        if (c == '\\' && i < line.size() && line[i] == '"') {
                            value += '"';
                            ++i;
                        } else if (c == '"') {
                            closed = true;
                            break;
                        } else {
                            value += c;
                        }
                    }
                    if (!closed) {
                        return false;
                    }
                }
                if (name.empty()) {
                    return false;
                }
                out.push_back(pair<string, string>(string(name), string(value)));
                if (i < line.size() && line[i] == ',') {
                    ++i;
                    continue;
                }
                break;
            }
            line.remove_prefix(i);
            return true;
        }

        inline bool is_key_type(std::string_view word) noexcept {
            KeyKind k;
            bool cert;
            return kind_of_name(word, k, cert);
        }

        // "YYYYMMDD[HHMM[SS]]" in the system's zone as seconds since 1970
        inline optional<int64_t> expiry_time(std::string_view v) {
            if (v.size() != 8 && v.size() != 12 && v.size() != 14) {
                return nullopt;
            }
            for (char c : v) {
                if (c < '0' || c > '9') {
                    return nullopt;
                }
            }
            auto num = [&](size_t at, size_t n) {
                int x = 0;
                for (size_t i = 0; i < n; ++i) {
                    x = x * 10 + (v[at + i] - '0');
                }
                return x;
            };
            int year = num(0, 4), month = num(4, 2), day = num(6, 2);
            int hour = v.size() >= 12 ? num(8, 2) : 0, minute = v.size() >= 12 ? num(10, 2) : 0, sec = v.size() == 14 ? num(12, 2) : 0;
            if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || sec > 59) {
                return nullopt;
            }
            std::tm tm{};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = sec;
            tm.tm_isdst = -1;
            return int64_t(::mktime(&tm));
        }
    }

    class authorized_keys;

    namespace detail {
        struct AuthorizedKeysState;
    }

    // The keys a server lets users in with: a handle of one word over the
    // lines read, its copies the same set; read-only once made, so safe
    // from many threads
    class authorized_keys {
    public:
        // A line: its key and its options in their order
        struct entry {
            ssh::public_key key;
            vector<pair<string, string>> options;

            // Whether the line has the option (its value: option())
            bool has(std::string_view name) const noexcept {
                for (const auto& o : options) {
                    if (o.first.view() == name) {
                        return true;
                    }
                }
                return false;
            }

            // The value of an option, empty when the line has none
            string option(std::string_view name) const noexcept {
                for (const auto& o : options) {
                    if (o.first.view() == name) {
                        return o.second;
                    }
                }
                return string();
            }
        };

        // An empty set
        authorized_keys() noexcept;

        // The lines of a file; lines that cannot be read passed over
        static expected<authorized_keys, io::error> load(const string& path) noexcept {
            auto text = io::read_text(path);
            if (!text) {
                return unexpected(text.error());
            }
            return parse(*text);
        }

        // The lines of a text
        static authorized_keys parse(const string& text) noexcept;

        // The line of a key: a plain key's own line (not a cert-authority
        // one), a certificate's authority's cert-authority line; nullopt
        // when there is none
        optional<entry> find(const ssh::public_key& key) const noexcept;

        // Whether the key lets the user in, as sshd decides it: a plain
        // key's line, or a certificate signed by a cert-authority line's key
        // that verifies, of the user type, valid now, whose principals hold
        // the user (or one of the line's principals="…" when it has them);
        // a line past its expiry-time lets no one in
        bool allows(const string& user, const ssh::public_key& key) const noexcept {
            auto e = find(key);
            if (!e) {
                return false;
            }
            const int64_t now = time::now().unix();
            if (e->has("expiry-time")) {
                auto t = detail::expiry_time(e->option("expiry-time").view());
                if (!t || now >= *t) {
                    return false;
                }
            }
            if (!key.is_certificate()) {
                return true;
            }
            auto c = key.certificate();
            if (!c || c->type != certificate_type::user || uint64_t(now) < c->valid_after || uint64_t(now) >= c->valid_before) {
                return false;
            }
            if (e->has("principals")) {
                std::string_view allowed = e->option("principals").view();
                for (const auto& p : c->principals) {
                    size_t from = 0;
                    while (from <= allowed.size()) {
                        size_t comma = allowed.find(',', from);
                        if (allowed.substr(from, comma == std::string_view::npos ? std::string_view::npos : comma - from) == p.view()) {
                            return true;
                        }
                        if (comma == std::string_view::npos) {
                            break;
                        }
                        from = comma + 1;
                    }
                }
                return false;
            }
            for (const auto& p : c->principals) {
                if (p == user) {
                    return true;
                }
            }
            return false;
        }

        // The lines held
        size_t size() const noexcept;

    private:
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT authorized_keys(sgcl::detail::FromWord, const tracked_ptr<detail::AuthorizedKeysState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::AuthorizedKeysState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::AuthorizedKeysState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::AuthorizedKeysState> _s;
    };

    namespace detail {
        struct AuthorizedKeysState {
            vector<authorized_keys::entry> entries;
        };

        // A line read into an entry; false for a comment, an empty line or
        // one that cannot be read
        inline bool read_authorized_line(std::string_view line, authorized_keys::entry& out) {
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
                line.remove_prefix(1);
            }
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.remove_suffix(1);
            }
            if (line.empty() || line.front() == '#') {
                return false;
            }
            size_t sp = line.find_first_of(" \t");
            std::string_view first = line.substr(0, sp);
            if (!is_key_type(first)) {
                if (!read_key_options(line, out.options)) {
                    return false;
                }
                while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
                    line.remove_prefix(1);
                }
            }
            auto key = ssh::public_key::parse(string(line));
            if (!key) {
                return false;
            }
            out.key = *key;
            return true;
        }
    }

    inline authorized_keys::authorized_keys() noexcept
    : _s(make_tracked<detail::AuthorizedKeysState>()) {
    }

    inline authorized_keys authorized_keys::parse(const string& text) noexcept {
        authorized_keys k;
        std::string_view v = text.view();
        size_t from = 0;
        while (from < v.size()) {
            size_t nl = v.find('\n', from);
            std::string_view line = v.substr(from, nl == std::string_view::npos ? std::string_view::npos : nl - from);
            entry e;
            if (detail::read_authorized_line(line, e)) {
                k._s->entries.push_back(std::move(e));
            }
            if (nl == std::string_view::npos) {
                break;
            }
            from = nl + 1;
        }
        return k;
    }

    inline optional<authorized_keys::entry> authorized_keys::find(const ssh::public_key& key) const noexcept {
        optional<ssh::certificate> cert = key.is_certificate() ? key.certificate() : nullopt;
        for (const auto& e : _s->entries) {
            const bool ca = e.has("cert-authority");
            if (!ca && e.key == key) {
                return e;
            }
            if (ca && cert && e.key == cert->signature_key) {
                return e;
            }
        }
        return nullopt;
    }

    inline size_t authorized_keys::size() const noexcept {
        return _s->entries.size();
    }
}
