//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "keys.h"
#include "../error.h"
#include "../ip.h"
#include "../../core/detail/handle_word.h"
#include "../../crypto/hmac.h"
#include "../../crypto/random.h"
#include "../../crypto/sha1.h"
#include "../../io/file.h"
#include "../../io/os.h"
#include "../../time/datetime.h"

#include <mutex>
#include <string>
#include <string_view>

// OpenSSH's known_hosts file (sshd(8), "SSH_KNOWN_HOSTS FILE FORMAT"): the
// host keys a client trusts, by the names of their hosts. A line is
// "[marker] patterns type base64 [comment]": the patterns a comma's list of
// names with * and ? and a ! in front to exclude, "[host]:port" for a port
// other than 22, or one name hashed ("|1|salt|HMAC-SHA1(salt, name)", what
// ssh-keygen -H writes); the marker @cert-authority for a key that signs
// the hosts' certificates, @revoked for a key never to be taken.
namespace sgcl::net::ssh {
    namespace detail {
        enum : uint8_t {
            KnownPlain = 0,
            KnownCertAuthority = 1,
            KnownRevoked = 2,
        };

        struct KnownEntry {
            uint8_t marker = KnownPlain;
            string patterns;
            ssh::public_key key;
        };

        struct KnownHostsState {
            std::mutex m;
            vector<KnownEntry> entries;
            string path;   // the file add() appends to; empty: none
        };

        SGCL_INLINE_HOT char lower(char c) noexcept {
            return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
        }

        // A pattern with * and ? over a name, blind to case
        inline bool glob(std::string_view p, std::string_view s) noexcept {
            size_t pi = 0, si = 0, star = std::string_view::npos, mark = 0;
            while (si < s.size()) {
                if (pi < p.size() && (p[pi] == '?' || lower(p[pi]) == lower(s[si]))) {
                    ++pi;
                    ++si;
                } else if (pi < p.size() && p[pi] == '*') {
                    star = pi++;
                    mark = si;
                } else if (star != std::string_view::npos) {
                    pi = star + 1;
                    si = ++mark;
                } else {
                    return false;
                }
            }
            while (pi < p.size() && p[pi] == '*') {
                ++pi;
            }
            return pi == p.size();
        }

        // The name a host is known by: "host" on port 22, "[host]:port" on
        // any other
        inline std::string known_name(std::string_view host, uint16_t port) {
            if (port == 22) {
                return std::string(host);
            }
            return "[" + std::string(host) + "]:" + std::to_string(port);
        }

        // "|1|salt|hash" against a name
        inline bool hashed_matches(std::string_view field, std::string_view name) {
            if (field.substr(0, 3) != "|1|") {
                return false;
            }
            std::string_view rest = field.substr(3);
            size_t bar = rest.find('|');
            if (bar == std::string_view::npos) {
                return false;
            }
            auto salt = encoding::base64::standard.decode(string(rest.substr(0, bar)));
            auto hash = encoding::base64::standard.decode(string(rest.substr(bar + 1)));
            if (!salt || !hash || hash->size() != 20) {
                return false;
            }
            auto mac = crypto::hmac<crypto::sha1>::of(slice<const byte>(reinterpret_cast<const byte*>(name.data()), name.size()), salt->as_slice());
            return std::memcmp(mac.data(), hash->data(), 20) == 0;
        }

        // Whether a line's patterns take the name: one of them matches and
        // none of the negated ones does
        inline bool patterns_match(std::string_view patterns, std::string_view name) {
            if (patterns.substr(0, 3) == "|1|") {
                return hashed_matches(patterns, name);
            }
            bool matched = false;
            size_t from = 0;
            while (from <= patterns.size()) {
                size_t comma = patterns.find(',', from);
                std::string_view p = patterns.substr(from, comma == std::string_view::npos ? std::string_view::npos : comma - from);
                if (!p.empty()) {
                    bool negated = p[0] == '!';
                    if (negated) {
                        p.remove_prefix(1);
                    }
                    if (glob(p, name)) {
                        if (negated) {
                            return false;
                        }
                        matched = true;
                    }
                }
                if (comma == std::string_view::npos) {
                    break;
                }
                from = comma + 1;
            }
            return matched;
        }

        // A line of the file read into an entry; false for a comment, an
        // empty line or one that cannot be read (passed over, as OpenSSH
        // passes them over)
        inline bool read_known_line(std::string_view line, KnownEntry& out) {
            auto skip = [&](size_t i) {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                    ++i;
                }
                return i;
            };
            auto word = [&](size_t& i) {
                size_t b = i;
                while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
                    ++i;
                }
                return line.substr(b, i - b);
            };
            size_t i = skip(0);
            if (i == line.size() || line[i] == '#' || line[i] == '\r') {
                return false;
            }
            std::string_view first = word(i);
            out.marker = KnownPlain;
            if (first == "@cert-authority") {
                out.marker = KnownCertAuthority;
            } else if (first == "@revoked") {
                out.marker = KnownRevoked;
            } else if (!first.empty() && first[0] == '@') {
                return false;
            }
            std::string_view patterns = first;
            if (out.marker != KnownPlain) {
                i = skip(i);
                patterns = word(i);
            }
            i = skip(i);
            auto key = ssh::public_key::parse(string(line.substr(i)));
            if (patterns.empty() || !key) {
                return false;
            }
            out.patterns = string(patterns);
            out.key = *key;
            return true;
        }

        inline void read_known(std::string_view text, vector<KnownEntry>& out) {
            size_t from = 0;
            while (from < text.size()) {
                size_t nl = text.find('\n', from);
                std::string_view line = text.substr(from, nl == std::string_view::npos ? std::string_view::npos : nl - from);
                KnownEntry e;
                if (read_known_line(line, e)) {
                    out.push_back(std::move(e));
                }
                if (nl == std::string_view::npos) {
                    break;
                }
                from = nl + 1;
            }
        }

        // "host:port" or "host" (port 22) taken apart
        inline bool host_and_port(const string& address, std::string& host, uint16_t& port) {
            std::string_view v = address.view();
            if (auto hp = split_host_port(v)) {
                auto p = parse_port(hp->port);
                if (!p || hp->host.empty()) {
                    return false;
                }
                host.assign(hp->host);
                port = *p;
                return true;
            }
            if (v.empty() || v.find(':') != std::string_view::npos) {
                return false;
            }
            host.assign(v);
            port = 22;
            return true;
        }

        inline io::error known_error(net::errc code, const string& address, std::string_view what) noexcept {
            return io::error(net::make_error_code(code), "ssh known_hosts", string(std::string(address.view()) + ": " + std::string(what)));
        }

        SGCL_INLINE_HOT bool same_kind(const ssh::public_key& a, const ssh::public_key& b) noexcept {
            return a.type() == b.type();
        }
    }

    // The host keys a client trusts: a handle of one word, its copies the
    // same set, safe from many threads. Loaded from a file (add() then
    // appends to it) or parsed from text
    class known_hosts {
    public:
        // An empty set, of no file
        known_hosts() noexcept
        : _s(make_tracked<detail::KnownHostsState>()) {
        }

        // The file of a user's: $HOME/.ssh/known_hosts
        static string default_path() noexcept {
            auto home = io::getenv(string("HOME"));
            return string(std::string(home ? home->view() : std::string_view()) + "/.ssh/known_hosts");
        }

        // The keys of a file (by default the user's); a file that is not
        // there is an empty set, which add() creates. Lines that cannot be
        // read are passed over
        static expected<known_hosts, io::error> load(const string& path = default_path()) noexcept {
            known_hosts k;
            k._s->path = path;
            auto text = io::read_text(path);
            if (!text) {
                if (text.error().is_not_found()) {
                    return k;
                }
                return unexpected(text.error());
            }
            detail::read_known(text->view(), k._s->entries);
            return k;
        }

        // The keys of a text in the file's format, of no file
        static known_hosts parse(const string& text) noexcept {
            known_hosts k;
            detail::read_known(text.view(), k._s->entries);
            return k;
        }

        // Whether the key is the host's: "host:port" or "host" (port 22).
        // A plain key matches a line of the host's names with that key; a
        // host certificate one whose @cert-authority line of the host's
        // names holds the authority that signed it, valid now, for the host,
        // of the host type. ssh_host_key_revoked for a key (or an authority)
        // a @revoked line names, ssh_host_key_mismatch when the host is
        // known with another key of the same type, ssh_host_key_unknown
        // otherwise; invalid_address for an address that is neither form
        expected<void, io::error> check(const string& address, const ssh::public_key& key) const noexcept {
            std::string host;
            uint16_t port;
            if (!detail::host_and_port(address, host, port)) {
                return unexpected(io::error(net::make_error_code(net::errc::invalid_address), "ssh known_hosts", address));
            }
            const std::string name = detail::known_name(host, port);
            optional<ssh::certificate> cert = key.is_certificate() ? key.certificate() : nullopt;
            bool known_other = false;
            bool ok = false;
            std::lock_guard<std::mutex> g(_s->m);
            for (const auto& e : _s->entries) {
                if (!detail::patterns_match(e.patterns.view(), name)) {
                    continue;
                }
                if (e.marker == detail::KnownRevoked) {
                    if (e.key == key || (cert && (e.key == cert->signature_key || e.key == cert->key))) {
                        return unexpected(detail::known_error(net::errc::ssh_host_key_revoked, address, "the key is revoked"));
                    }
                    continue;
                }
                if (e.marker == detail::KnownCertAuthority) {
                    if (cert && e.key == cert->signature_key &&
                        detail::certificate_holds(*cert, certificate_type::host, host, uint64_t(time::now().unix()))) {
                        ok = true;
                    }
                    continue;
                }
                if (e.key == key) {
                    ok = true;
                } else if (detail::same_kind(e.key, key)) {
                    known_other = true;
                }
            }
            if (ok) {
                return {};
            }
            if (known_other) {
                return unexpected(detail::known_error(net::errc::ssh_host_key_mismatch, address, "the host is known with another key"));
            }
            return unexpected(detail::known_error(net::errc::ssh_host_key_unknown, address, "the host is not known"));
        }

        // A line for the host and the key added, and appended to the file
        // the set was loaded from (made when it is not there); hashed, its
        // name is written as ssh-keygen -H writes it
        expected<void, io::error> add(const string& address, const ssh::public_key& key, bool hashed = false) const noexcept {
            std::string host;
            uint16_t port;
            if (!detail::host_and_port(address, host, port)) {
                return unexpected(io::error(net::make_error_code(net::errc::invalid_address), "ssh known_hosts", address));
            }
            std::string name = detail::known_name(host, port);
            if (hashed) {
                vector<byte> salt = crypto::random::bytes(20);
                auto mac = crypto::hmac<crypto::sha1>::of(slice<const byte>(reinterpret_cast<const byte*>(name.data()), name.size()), salt.as_slice());
                name = "|1|" + std::string(encoding::base64::standard.encode(salt.as_slice()).view()) + "|" +
                       std::string(encoding::base64::standard.encode(slice<const byte>(mac.data(), mac.size())).view());
            }
            detail::KnownEntry e;
            e.patterns = string(name);
            e.key = key.with_comment(string());
            std::string line = name + " " + std::string(e.key.to_string().view()) + "\n";
            std::lock_guard<std::mutex> g(_s->m);
            if (!_s->path.empty()) {
                auto f = io::open(_s->path, io::open_flags::write | io::open_flags::create | io::open_flags::append, io::permissions(0600));
                if (!f) {
                    return unexpected(f.error());
                }
                auto w = f->write(string(line));
                if (!w) {
                    (void)f->close();
                    return unexpected(w.error());
                }
                if (auto c = f->close(); !c) {
                    return c;
                }
            }
            _s->entries.push_back(std::move(e));
            return {};
        }

        // The lines held, as the file would have them (comments and lines
        // that could not be read left out)
        string to_string() const noexcept {
            std::string out;
            std::lock_guard<std::mutex> g(_s->m);
            for (const auto& e : _s->entries) {
                if (e.marker == detail::KnownCertAuthority) {
                    out += "@cert-authority ";
                } else if (e.marker == detail::KnownRevoked) {
                    out += "@revoked ";
                }
                out.append(e.patterns.view());
                out += ' ';
                out.append(e.key.to_string().view());
                out += '\n';
            }
            return string(out);
        }

        // The lines held
        size_t size() const noexcept {
            std::lock_guard<std::mutex> g(_s->m);
            return _s->entries.size();
        }

        // The file add() appends to; empty for a set parsed from text
        string path() const noexcept {
            std::lock_guard<std::mutex> g(_s->m);
            return _s->path;
        }

    private:
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT known_hosts(sgcl::detail::FromWord, const tracked_ptr<detail::KnownHostsState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::KnownHostsState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::KnownHostsState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::KnownHostsState> _s;
    };
}
