//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie.h"
#include "request.h"
#include "response_writer.h"
#include "server.h"
#include "status.h"
#include "detail/server_state.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/chacha20_poly1305.h"
#include "../../crypto/random.h"
#include "../../crypto/secret.h"
#include "../../encoding/base64.h"
#include "../../slog/logger.h"
#include "../../time/datetime.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// Sessions of a server (net::http::sessions, a middleware, and
// net::http::session, the session of one request): string values by name
// kept between the requests of one client by a cookie, either sealed in the
// cookie itself (XChaCha20-Poly1305 under the program's key) or on the
// server, in memory, by a random id the cookie carries.
namespace sgcl::net::http {
    namespace detail {
        // The session of one request: its values as loaded, what the
        // handler did to them, and where they came from
        struct SessionState {
            vector<pair<string, string>> values;
            string id;                  // the memory store's; "" for a new one and for the cookie store
            int64_t created = 0;        // unix seconds
            bool loaded = false;        // from a cookie the client sent
            bool changed = false;
            bool renewed = false;
            bool destroyed = false;

            SGCL_INLINE_HOT const string* find(std::string_view key) const noexcept {
                for (auto& v : values) {
                    if (v.first.view() == key) {
                        return &v.second;
                    }
                }
                return nullptr;
            }
        };

        // A session kept on the server
        struct MemorySession {
            vector<pair<string, string>> values;
            int64_t created = 0;        // unix seconds
            int64_t touched = 0;
        };

        // What a sessions middleware keeps: its options, its keys (in plain
        // memory, zeroed when the state goes) or its sessions
        struct SessionsState {
            string cookie;
            duration max_age;
            duration idle_timeout;
            string path;
            string domain;
            bool secure = true;
            bool http_only = true;
            string same_site;
            bool in_cookie = false;
            std::unique_ptr<std::vector<crypto::xchacha20_poly1305>> keys;   // the first seals, all open
            std::mutex lock;                                                    // the memory store's
            map<string, tracked_ptr<MemorySession>> sessions;
            size_t swept_at = 0;        // the count at the last sweep

            SGCL_INLINE_HOT int64_t max_age_seconds() const noexcept {
                return max_age.nanoseconds() / 1000000000;
            }

            SGCL_INLINE_HOT int64_t idle_seconds() const noexcept {
                return idle_timeout.nanoseconds() / 1000000000;
            }

            // The memory store's expired sessions dropped, when its count
            // has doubled since the last sweep (under the lock)
            void sweep(int64_t now) {
                if (sessions.size() < 2 * swept_at + 64) {
                    return;
                }
                vector<string> gone;
                for (auto& [id, s] : sessions) {
                    if (expired(*s, now)) {
                        gone.push_back(id);
                    }
                }
                for (auto& id : gone) {
                    sessions.erase(id);
                }
                swept_at = sessions.size();
            }

            SGCL_INLINE_HOT bool expired(const MemorySession& s, int64_t now) const noexcept {
                return (max_age > duration::zero() && now - s.created >= max_age_seconds()) ||
                       (idle_timeout > duration::zero() && now - s.touched >= idle_seconds());
            }
        };

        inline constexpr char SessionKey = 0;   // the address the request's session hangs on

        inline int64_t session_now() noexcept {
            return time::now().unix();
        }

        // The cookie store's plaintext: a version byte, the start and the
        // last use (8 bytes each, little-endian), then each pair as two
        // lengths (4 bytes) and their bytes
        inline std::string pack_session(const SessionState& s, int64_t touched) {
            std::string out;
            out += char(1);
            auto put64 = [&](uint64_t v) {
                for (int i = 0; i < 8; ++i) {
                    out += char(v >> (8 * i));
                }
            };
            auto put32 = [&](uint32_t v) {
                for (int i = 0; i < 4; ++i) {
                    out += char(v >> (8 * i));
                }
            };
            put64(uint64_t(s.created));
            put64(uint64_t(touched));
            for (auto& [k, v] : s.values) {
                put32(uint32_t(k.size()));
                out += k.view();
                put32(uint32_t(v.size()));
                out += v.view();
            }
            return out;
        }

        // What pack_session made, read back: false for anything else (the
        // tag held, so only a key of ours made it; a version of another
        // build is no session)
        inline bool unpack_session(std::string_view in, SessionState& s, int64_t& touched) {
            size_t at = 0;
            auto get = [&](size_t n, uint64_t& v) {
                if (in.size() - at < n) {
                    return false;
                }
                v = 0;
                for (size_t i = 0; i < n; ++i) {
                    v |= uint64_t(uint8_t(in[at + i])) << (8 * i);
                }
                at += n;
                return true;
            };
            uint64_t version = 0, created = 0, last = 0;
            if (!get(1, version) || version != 1 || !get(8, created) || !get(8, last)) {
                return false;
            }
            s.created = int64_t(created);
            touched = int64_t(last);
            while (at < in.size()) {
                uint64_t kn = 0, vn = 0;
                if (!get(4, kn) || in.size() - at < kn) {
                    return false;
                }
                std::string_view k = in.substr(at, size_t(kn));
                at += size_t(kn);
                if (!get(4, vn) || in.size() - at < vn) {
                    return false;
                }
                std::string_view v = in.substr(at, size_t(vn));
                at += size_t(vn);
                s.values.push_back(pair<string, string>(string(k), string(v)));
            }
            return true;
        }

        // A random id of 128 bits, base64url without padding (22 characters)
        inline string session_id() {
            byte raw[16];
            crypto::random::fill(slice<byte>(raw, sizeof raw));
            return encoding::base64::raw_url.encode(slice<const byte>(raw, sizeof raw));
        }

        // The session of a request loaded from its cookie: the memory
        // store's by its id (expired ones dropped), the cookie store's
        // opened by any of the keys (the cookie's name the associated data)
        // and checked against the times; a new session otherwise
        inline tracked_ptr<SessionState> load_session(SessionsState& st, const RequestImpl& req) {
            tracked_ptr s = make_tracked<SessionState>();
            const int64_t now = session_now();
            s->created = now;
            auto value = request_cookie(req.fields, st.cookie.view());
            if (!value || value->empty()) {
                return s;
            }
            if (st.in_cookie) {
                auto sealed = encoding::base64::raw_url.decode(*value);
                if (!sealed) {
                    return s;
                }
                const slice<const byte> aad(reinterpret_cast<const byte*>(st.cookie.data()), st.cookie.size());
                for (auto& key : *st.keys) {
                    auto opened = key.open_random(slice<const byte>(*sealed), aad);
                    if (!opened) {
                        continue;
                    }
                    tracked_ptr fresh = make_tracked<SessionState>();
                    int64_t touched = 0;
                    if (!unpack_session(std::string_view(reinterpret_cast<const char*>(opened->data()), opened->size()), *fresh, touched)) {
                        return s;
                    }
                    MemorySession times;
                    times.created = fresh->created;
                    times.touched = touched;
                    if (st.expired(times, now) || fresh->created > now + 60) {
                        return s;   // past its time, or from a clock that is wrong
                    }
                    fresh->loaded = true;
                    return fresh;
                }
                return s;
            }
            std::lock_guard<std::mutex> g(st.lock);
            auto it = st.sessions.find(*value);
            if (it == st.sessions.end()) {
                return s;
            }
            if (st.expired(*it->second, now)) {
                st.sessions.erase(it);
                return s;
            }
            it->second->touched = now;
            s->values = it->second->values;
            s->created = it->second->created;
            s->id = *value;
            s->loaded = true;
            return s;
        }

        // The Set-Cookie of a session: its value and Max-Age (zero: the
        // cookie expired), the options' attributes
        inline void set_session_cookie(const SessionsState& st, WriterImpl& w, const string& value, optional<int64_t> max_age) {
            cookie c(st.cookie, value);
            c.path = st.path;
            c.domain = st.domain;
            c.secure = st.secure;
            c.http_only = st.http_only;
            c.same_site = st.same_site;
            if (max_age) {
                c.max_age = duration(std::chrono::seconds(*max_age > 0 ? *max_age : 0));
            }
            w.fields.add("Set-Cookie", c.to_string());
        }

        // The session saved as the head is about to be made: a cookie for a
        // session changed, renewed, new with values, or (an idle timeout)
        // used; an expired cookie for one destroyed that the client had; in
        // memory, the values written back and an old id dropped. A session
        // new and empty sets nothing: no cookie for every visitor
        inline void save_session(SessionsState& st, SessionState& s, WriterImpl& w) {
            const int64_t now = session_now();
            if (s.destroyed) {
                if (!st.in_cookie && !s.id.empty()) {
                    std::lock_guard<std::mutex> g(st.lock);
                    st.sessions.erase(s.id);
                }
                if (s.loaded) {
                    set_session_cookie(st, w, string(), int64_t(0));
                }
                return;
            }
            const bool touch = st.idle_timeout > duration::zero() && s.loaded;
            if (!s.changed && !s.renewed && !touch) {
                return;
            }
            if (s.values.empty() && !s.loaded) {
                return;
            }
            // the cookie lives to the session's end: its Max-Age what is left
            // of max_age; none (the browser's session) when max_age is zero
            optional<int64_t> left;
            if (st.max_age > duration::zero()) {
                left = s.created + st.max_age_seconds() - now;
            }
            if (st.in_cookie) {
                const std::string plain = pack_session(s, now);
                const slice<const byte> aad(reinterpret_cast<const byte*>(st.cookie.data()), st.cookie.size());
                auto sealed = (*st.keys)[0].seal_random(slice<const byte>(reinterpret_cast<const byte*>(plain.data()), plain.size()), aad);
                string value = encoding::base64::raw_url.encode(slice<const byte>(sealed));
                if (value.size() + st.cookie.size() > 4000) {
                    // past what a browser keeps of a cookie (RFC 6265 §6.1: 4096 for the name, the value and the attributes)
                    slog::default_logger().error("http session too large for its cookie", "bytes", uint64_t(value.size()));
                    return;
                }
                set_session_cookie(st, w, value, left);
                return;
            }
            if (!s.changed && !s.renewed) {
                return;   // used: its time moved by load_session; the cookie as it was
            }
            std::lock_guard<std::mutex> g(st.lock);
            if (s.renewed && !s.id.empty()) {
                st.sessions.erase(s.id);
                s.id = string();
            }
            const bool fresh_id = s.id.empty();
            if (fresh_id) {
                s.id = session_id();
            }
            tracked_ptr entry = make_tracked<MemorySession>();
            entry->values = s.values;
            entry->created = s.created;
            entry->touched = now;
            st.sessions[s.id] = entry;
            st.sweep(now);
            if (fresh_id || !s.loaded) {
                set_session_cookie(st, w, s.id, left);
            }
        }
    }

    // The session of a request, as the sessions middleware loaded it:
    // string values by name. A handle of one word over the request's own
    // state: what the handler sets is saved as the response's head is made
    // (a cookie, or the server's store), so a handler that streams keeps its
    // session too. One request's session is its handler's: not to be
    // changed by two tasks at once
    class session {
    public:
        // The session of the request; invalid_argument when no sessions
        // middleware ran for it
        explicit session(const request& req)
        : _s(detail::RequestAccess::impl(req)->attachment(&detail::SessionKey).template as<detail::SessionState>()) {
            if (!_s) {
                throw invalid_argument("http::session: no sessions middleware ran for the request");
            }
        }

        // The value of the key, "" when there is none
        SGCL_INLINE_HOT string get(const string& key) const noexcept {
            auto v = _s->find(key.view());
            return v ? *v : string();
        }

        SGCL_INLINE_HOT bool contains(const string& key) const noexcept {
            return _s->find(key.view()) != nullptr;
        }

        void set(const string& key, const string& value) {
            for (auto& v : _s->values) {
                if (v.first == key) {
                    v.second = value;
                    _s->changed = true;
                    return;
                }
            }
            _s->values.push_back(pair<string, string>(key, value));
            _s->changed = true;
        }

        void erase(const string& key) {
            for (size_t i = 0; i < _s->values.size(); ++i) {
                if (_s->values[i].first == key) {
                    _s->values.erase(_s->values.begin() + std::ptrdiff_t(i));
                    _s->changed = true;
                    return;
                }
            }
        }

        // Every value gone; the session goes on (destroy() ends it)
        void clear() noexcept {
            if (!_s->values.empty()) {
                _s->values.clear();
                _s->changed = true;
            }
        }

        // The server store's id, "" for the cookie store and for a session
        // not yet saved
        SGCL_INLINE_HOT string id() const noexcept {
            return _s->id;
        }

        // Whether the request came without a session (or with one past its
        // time)
        SGCL_INLINE_HOT bool is_new() const noexcept {
            return !_s->loaded;
        }

        // A new identity for the same values, after a login, so that an id
        // an attacker planted before it is worth nothing (session fixation):
        // a new id in the server's store, the old one dropped; a cookie
        // sealed anew
        void renew() noexcept {
            _s->renewed = true;
            _s->destroyed = false;
        }

        // The session ended: its values gone, its cookie expired, a server
        // store's entry dropped
        void destroy() noexcept {
            _s->values.clear();
            _s->destroyed = true;
        }

    private:
        tracked_ptr<detail::SessionState> _s;
    };

    // The sessions of a server's requests (a middleware, server::use): each
    // request's session loaded from its cookie before the handler, saved as
    // the head is made. Values in the cookie itself, sealed under the
    // program's key (in_cookie), or on the server in memory by a random id
    // (in_memory). Secure defaults: the cookie __Host-session, Secure,
    // HttpOnly, SameSite=Lax, Path=/, 24 hours from the session's start.
    // A handle of one word: copies share the store
    class sessions {
    public:
        struct options {
            string cookie = "__Host-session";          // the cookie's name; a __Host- name needs secure, path "/" and no domain
            duration max_age = std::chrono::hours(24);   // from the session's start; the cookie's Max-Age; zero: a cookie of the browser's session
            duration idle_timeout = duration::zero();  // from its last request; zero: none
            string path = "/";
            string domain;                             // "" for the host alone
            bool secure = true;                        // Secure: over https alone (browsers allow it on localhost)
            bool http_only = true;                     // HttpOnly: no script reads it
            string same_site = "Lax";                  // Lax, Strict, None (None needs secure) or "" for none
        };

        // Values sealed in the cookie (XChaCha20-Poly1305, a random nonce,
        // the cookie's name as associated data, the start and the last use
        // inside) under a key of 32 bytes. A sealed session past about 4 KB
        // is not set (the values are a cookie's: a few small strings), and
        // an error goes to slog. invalid_argument for a key of another
        // length, or options a browser would refuse
        static sessions in_cookie(const crypto::secret_bytes& key) {
            return in_cookie(key, options());
        }

        static sessions in_cookie(const crypto::secret_bytes& key, const options& o) {
            sessions out(o);
            out._s->in_cookie = true;
            out._s->keys = std::make_unique<std::vector<crypto::xchacha20_poly1305>>();
            out._s->keys->push_back(_key(key));
            return out;
        }

        // The same, cookies sealed under the previous key opened too: a key
        // rotated without ending every session at once
        static sessions in_cookie(const crypto::secret_bytes& key, const crypto::secret_bytes& previous, const options& o) {
            sessions out = in_cookie(key, o);
            out._s->keys->push_back(_key(previous));
            return out;
        }

        // Values on the server, in memory, by an id of 128 random bits in
        // the cookie; sessions past their time dropped as they are met and
        // swept as the store grows. They end with the process
        static sessions in_memory() {
            return sessions(options());
        }

        static sessions in_memory(const options& o) {
            return sessions(o);
        }

        // How many sessions the server's store holds (0 for the cookie store)
        size_t size() const {
            std::lock_guard<std::mutex> g(_s->lock);
            return _s->sessions.size();
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        explicit sessions(const options& o)
        : _s(make_tracked<detail::SessionsState>()) {
            const std::string_view name = o.cookie.view();
            if (name.empty() || !detail::is_token(name)) {
                throw invalid_argument("http::sessions: the cookie's name is a token");
            }
            if (name.starts_with("__Host-") && (!o.secure || o.path != "/" || !o.domain.empty())) {
                throw invalid_argument("http::sessions: a __Host- cookie is secure, of the path / and of no domain");
            }
            if (name.starts_with("__Secure-") && !o.secure) {
                throw invalid_argument("http::sessions: a __Secure- cookie is secure");
            }
            if (o.same_site == "None" && !o.secure) {
                throw invalid_argument("http::sessions: SameSite=None needs secure");
            }
            _s->cookie = o.cookie;
            _s->max_age = o.max_age;
            _s->idle_timeout = o.idle_timeout;
            _s->path = o.path;
            _s->domain = o.domain;
            _s->secure = o.secure;
            _s->http_only = o.http_only;
            _s->same_site = o.same_site;
        }

        static crypto::xchacha20_poly1305 _key(const crypto::secret_bytes& k) {
            if (k.size() != crypto::xchacha20_poly1305::key_size) {
                throw invalid_argument("http::sessions: a key is 32 bytes");
            }
            return crypto::xchacha20_poly1305(k.as_slice());
        }

        detail::Step _wrap(detail::Step next) const {
            return [st = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                auto& req = detail::RequestAccess::impl(r);
                tracked_ptr<detail::SessionState> s = detail::load_session(*st, *req);
                req->attach(&detail::SessionKey, s);
                detail::add_before_head(*detail::WriterAccess::impl(w), [st, s](detail::WriterImpl& wi) {
                    detail::save_session(*st, *s, wi);
                });
                return next(r, w);
            };
        }

        tracked_ptr<detail::SessionsState> _s;
    };
}
