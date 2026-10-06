//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "request.h"
#include "response_writer.h"
#include "server.h"
#include "status.h"
#include "detail/auth.h"
#include "detail/server_state.h"
#include "detail/wire.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/constant_time.h"
#include "../../crypto/hmac.h"
#include "../../crypto/random.h"
#include "../../crypto/sha256.h"
#include "../../encoding/base64.h"
#include "../../time/datetime.h"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

// The authentications of a server (middlewares): Basic (RFC 7617) with a
// function of the program's that verifies a user and a password, and
// Digest (RFC 7616: SHA-256, SHA-512/256, MD5 for legacy clients, their
// -sess forms, qop auth and auth-int) with one that gives a user's
// password. The user let in is the request's authenticated_user().
namespace sgcl::net::http {
    namespace detail {
        // The 401 of an authentication: its challenges, a text body
        inline void unauthorized(response_writer& w, const vector<string>& challenges) {
            for (auto& c : challenges) {
                w.add_header("WWW-Authenticate", c);
            }
            w.error(status::unauthorized);
        }

        struct BasicAuthState {
            string realm;
            string challenge;
            function<bool(const string&, const string&)> verify;
        };

        // What a digest_auth keeps: its options, its key (the nonces' HMAC),
        // and the counts of the nonces in use (replay of a count refused)
        struct DigestAuthState {
            string realm;
            vector<DigestAlgorithm> algorithms;
            bool auth_int = false;
            int64_t lifetime_ms = 300000;
            function<optional<string>(const string&)> password;
            std::unique_ptr<std::array<byte, 32>> key = std::make_unique<std::array<byte, 32>>();   // in plain memory
            string opaque;
            std::mutex lock;
            std::unordered_map<std::string, uint64_t> counts;   // nonce -> the highest nc seen
            int64_t swept_ms = 0;

            // A nonce: the time it was made (8 bytes, milliseconds), 8 random
            // bytes (two challenges of one millisecond differ), and the first
            // 16 bytes of the HMAC-SHA256 of both and the realm under the key,
            // base64url: stateless, its age read back from it
            static constexpr size_t NonceBytes = 32;

            array<byte, 16> nonce_mac(const byte* head) const {
                std::string msg(reinterpret_cast<const char*>(head), 16);
                msg += realm.view();
                auto mac = crypto::hmac<crypto::sha256>::of(slice<const byte>(reinterpret_cast<const byte*>(msg.data()), msg.size()),
                                                             slice<const byte>(key->data(), key->size()));
                array<byte, 16> out;
                for (size_t i = 0; i < 16; ++i) {
                    out[i] = mac[i];
                }
                return out;
            }

            string make_nonce(int64_t now_ms) const {
                byte raw[NonceBytes];
                for (int i = 0; i < 8; ++i) {
                    raw[i] = byte(uint64_t(now_ms) >> (8 * (7 - i)));
                }
                crypto::random::fill(slice<byte>(raw + 8, 8));
                auto mac = nonce_mac(raw);
                for (size_t i = 0; i < 16; ++i) {
                    raw[16 + i] = mac[i];
                }
                return encoding::base64::raw_url.encode(slice<const byte>(raw, sizeof raw));
            }

            // Whether a nonce is one of ours (its mac compared in constant
            // time), and the time it was made
            optional<int64_t> nonce_time(std::string_view nonce) const {
                auto raw = encoding::base64::raw_url.decode(string(nonce));
                if (!raw || raw->size() != NonceBytes) {
                    return nullopt;
                }
                int64_t t = 0;
                for (int i = 0; i < 8; ++i) {
                    t = (t << 8) | int64_t(uint8_t((*raw)[size_t(i)]));
                }
                auto mac = nonce_mac(raw->data());
                if (!crypto::constant_time::equal(slice<const byte>(mac.data(), 16), slice<const byte>(raw->data() + 16, 16))) {
                    return nullopt;
                }
                return t;
            }

            // The nonce's count taken: false when it is not above the last
            // one seen (a request replayed). Counts of nonces past their
            // lifetime are dropped now and then
            bool take_count(const std::string& nonce, uint64_t nc, int64_t now_ms) {
                std::lock_guard<std::mutex> g(lock);
                if (now_ms - swept_ms > lifetime_ms) {
                    for (auto it = counts.begin(); it != counts.end();) {
                        auto t = nonce_time(it->first);
                        if (!t || now_ms - *t > lifetime_ms) {
                            it = counts.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    swept_ms = now_ms;
                }
                auto& last = counts[nonce];
                if (nc <= last) {
                    return false;
                }
                last = nc;
                return true;
            }

            // The challenges of a 401: one a algorithm, in the options' order
            vector<string> challenges(int64_t now_ms, bool stale) const {
                vector<string> out;
                const string nonce = make_nonce(now_ms);
                for (auto& a : algorithms) {
                    std::string c = "Digest realm=";
                    append_quoted(c, realm.view());
                    c += auth_int ? ", qop=\"auth, auth-int\"" : ", qop=\"auth\"";
                    c += ", algorithm=";
                    c += digest_algorithm_name(a);
                    c += ", nonce=\"";
                    c += nonce.view();
                    c += "\", opaque=\"";
                    c += opaque.view();
                    c += "\", charset=UTF-8";
                    if (stale) {
                        c += ", stale=true";
                    }
                    out.push_back(string(std::string_view(c)));
                }
                return out;
            }
        };

        inline int64_t auth_now_ms() noexcept {
            return time::now().unix_milli();
        }

        // The request-target as it came (Digest's uri must be it): the
        // head's, or of a request made by test_request, its URL's path and
        // query
        inline std::string request_target(const RequestImpl& req) {
            if (!req.target.empty()) {
                return std::string(req.target.view());
            }
            std::string out = "/";
            if (auto u = const_cast<RequestImpl&>(req).url_of()) {
                out.assign(u->path().view());
                if (u->has_query()) {
                    out += '?';
                    out += u->query().view();
                }
            }
            return out;
        }

        // The verdict of a Digest's credentials
        struct DigestVerdict {
            bool ok = false;
            bool stale = false;
            std::string user;
        };

        // The credentials of an Authorization: Digest checked (RFC 7616
        // §3.4): the realm ours, the nonce ours and young, the uri the
        // request's target, the algorithm and the qop offered, the count
        // above the last, the response the one the password makes (compared
        // in constant time). `body_hash` is H(body) for auth-int, computed by
        // the caller when the qop asks for it
        inline DigestVerdict check_digest(DigestAuthState& st, const AuthChallenge& c, std::string_view method, std::string_view target,
                                          std::string_view body_hash) {
            DigestVerdict out;
            auto get = [&](std::string_view n) -> std::string_view {
                auto p = c.param(n);
                return p ? std::string_view(*p) : std::string_view();
            };
            std::string user;
            if (auto star = c.param("username*")) {
                auto u = decode_ext_value(*star);
                if (!u) {
                    return out;
                }
                user = std::move(*u);
            } else if (auto u = c.param("username")) {
                user = *u;
            } else {
                return out;
            }
            if (iequal(get("userhash"), "true")) {
                return out;   // never offered: the name cannot be looked up from its hash
            }
            if (get("realm") != st.realm.view()) {
                return out;
            }
            const std::string_view nonce = get("nonce");
            auto made = st.nonce_time(nonce);
            if (!made) {
                return out;
            }
            const int64_t now = auth_now_ms();
            const std::string_view algorithm_text = get("algorithm");
            auto algorithm = digest_algorithm(algorithm_text.empty() ? std::string_view("MD5") : algorithm_text);
            if (!algorithm) {
                return out;
            }
            bool offered = false;
            for (auto& a : st.algorithms) {
                offered = offered || (a.hash == algorithm->hash && a.sess == algorithm->sess);
            }
            const std::string_view qop = get("qop");
            if (!offered || !(qop == "auth" || (qop == "auth-int" && st.auth_int))) {
                return out;
            }
            if (get("uri") != target || (c.param("opaque") && get("opaque") != st.opaque.view())) {
                return out;
            }
            const std::string_view nc_text = get("nc");
            uint64_t nc = 0;
            if (nc_text.size() != 8) {
                return out;
            }
            for (char ch : nc_text) {
                const int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
                if (d < 0) {
                    return out;
                }
                nc = nc * 16 + uint64_t(d);
            }
            const std::string_view cnonce = get("cnonce");
            if (cnonce.empty()) {
                return out;
            }
            auto password = st.password(string(std::string_view(user)));
            if (!password) {
                return out;
            }
            DigestInput in;
            in.algorithm = *algorithm;
            in.user = user;
            in.realm = st.realm.view();
            in.password = password->view();
            in.method = method;
            in.uri = target;
            in.nonce = nonce;
            in.cnonce = cnonce;
            in.nc = nc_text;
            in.qop = qop;
            in.body_hash = body_hash;
            const std::string expected = digest_response(in);
            const std::string_view sent = get("response");
            if (!crypto::constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(expected.data()), expected.size()),
                                              slice<const byte>(reinterpret_cast<const byte*>(sent.data()), sent.size()))) {
                return out;
            }
            if (now - *made > st.lifetime_ms) {
                out.stale = true;   // the right password on a nonce past its time: a new nonce, no new password
                return out;
            }
            if (!st.take_count(std::string(nonce), nc, now)) {
                return out;
            }
            out.ok = true;
            out.user = std::move(user);
            return out;
        }

        // auth-int: the body read whole (within the server's limit) and
        // hashed, then put back as the request's body for the handler
        inline async::task<> digest_with_body(tracked_ptr<DigestAuthState> st, Step next, request r, response_writer w, AuthChallenge c,
                                              DigestAlgorithm a) {
            auto& req = RequestAccess::impl(r);
            auto bytes = co_await r.async_bytes();
            if (!bytes) {
                w.error(req->body && req->body->error_status() == 413 ? status::content_too_large : status::bad_request);
                co_return;
            }
            const std::string_view body(reinterpret_cast<const char*>(bytes->data()), bytes->size());
            const std::string hash = digest_hex(a.hash, body);
            auto verdict = check_digest(*st, c, req->method.view(), request_target(*req), hash);
            if (!verdict.ok) {
                unauthorized(w, st->challenges(auth_now_ms(), verdict.stale));
                co_return;
            }
            // the body again, from memory
            auto ends = net::connection::in_memory();
            tracked_ptr wire = make_tracked<Wire>(ends.first);
            wire->preload(body);
            req->body = make_tracked<Body>(wire, BodyFraming{Framing::length, bytes->size()}, 0, false);
            req->attach(&AuthUserKey, make_tracked<string>(std::string_view(verdict.user)));
            if (auto t = next(r, w)) {
                co_await *t;
            }
        }
    }

    // Basic authentication of a server (RFC 7617), a middleware: a request
    // whose Authorization: Basic the program's verify takes goes on, its
    // user the request's authenticated_user(); any other is 401 with WWW-Authenticate:
    // Basic realm="...", charset="UTF-8". The password goes in clear text:
    // over https alone. verify runs on the worker as the handler would; a
    // slow hash (bcrypt, argon2) is the program's to call there
    //
    //     srv.use(net::http::basic_auth("admin", [](const string& u, const string& p) { return check(u, p); }));
    //
    // A handle of one word: copies share the settings
    class basic_auth {
    public:
        basic_auth(const string& realm, function<bool(const string& user, const string& password)> verify)
        : _s(make_tracked<detail::BasicAuthState>()) {
            _s->realm = realm;
            std::string c = "Basic realm=";
            detail::append_quoted(c, realm.view());
            c += ", charset=\"UTF-8\"";
            _s->challenge = string(std::string_view(c));
            _s->verify = std::move(verify);
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [st = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                auto& req = detail::RequestAccess::impl(r);
                if (auto field = detail::HeadersAccess::find(req->fields, "authorization")) {
                    if (auto up = detail::read_basic(*field)) {
                        const string user(std::string_view(up->first));
                        if (st->verify(user, string(std::string_view(up->second)))) {
                            req->attach(&detail::AuthUserKey, make_tracked<string>(user));
                            return next(r, w);
                        }
                    }
                }
                detail::unauthorized(w, vector<string>{st->challenge});
                return nullopt;
            };
        }

        tracked_ptr<detail::BasicAuthState> _s;
    };

    // Digest authentication of a server (RFC 7616), a middleware: the
    // password never crosses the network, only a hash of it with a nonce of
    // the server's and one of the client's. A 401 offers one challenge an
    // algorithm (SHA-256 and MD5 by default: MD5 for the clients of before
    // 2015; SHA-512/256 and the -sess forms when asked), qop auth (and
    // auth-int, when asked: the body hashed too, read whole before the
    // handler), a nonce of 5 minutes (stateless: its time and an HMAC of
    // it), stale=true for the right password on an old nonce. The counts
    // of a nonce must grow: a request replayed is refused. The user let in
    // is the request's authenticated_user()
    //
    //     srv.use(net::http::digest_auth("files", [](const string& user) { return password_of(user); }));
    //
    // A handle of one word: copies share the settings and the nonces' key
    class digest_auth {
    public:
        enum class algorithm : uint8_t {
            md5,
            md5_sess,
            sha256,
            sha256_sess,
            sha512_256,
            sha512_256_sess,
        };

        struct options {
            vector<algorithm> algorithms = {algorithm::sha256, algorithm::md5};   // the challenges of a 401, in this order
            bool auth_int = false;                     // qop auth-int offered beside auth: the body read whole first
            duration nonce_lifetime = std::chrono::minutes(5);   // a nonce older is stale: a new one, the same password
        };

        digest_auth(const string& realm, function<optional<string>(const string& user)> password)
        : digest_auth(realm, std::move(password), options()) {
        }

        // invalid_argument for options without an algorithm
        digest_auth(const string& realm, function<optional<string>(const string& user)> password, const options& o)
        : _s(make_tracked<detail::DigestAuthState>()) {
            if (o.algorithms.empty()) {
                throw invalid_argument("http::digest_auth: no algorithm");
            }
            _s->realm = realm;
            for (auto a : o.algorithms) {
                detail::DigestAlgorithm d;
                d.hash = a == algorithm::md5 || a == algorithm::md5_sess ? detail::DigestHash::md5
                         : a == algorithm::sha256 || a == algorithm::sha256_sess ? detail::DigestHash::sha256
                                                                                 : detail::DigestHash::sha512_256;
                d.sess = a == algorithm::md5_sess || a == algorithm::sha256_sess || a == algorithm::sha512_256_sess;
                _s->algorithms.push_back(d);
            }
            _s->auth_int = o.auth_int;
            _s->lifetime_ms = o.nonce_lifetime.milliseconds() > 0 ? o.nonce_lifetime.milliseconds() : 1;
            _s->password = std::move(password);
            crypto::random::fill(slice<byte>(_s->key->data(), _s->key->size()));
            byte opaque[12];
            crypto::random::fill(slice<byte>(opaque, sizeof opaque));
            _s->opaque = encoding::base64::raw_url.encode(slice<const byte>(opaque, sizeof opaque));
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [st = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                auto& req = detail::RequestAccess::impl(r);
                bool stale = false;
                if (auto field = detail::HeadersAccess::find(req->fields, "authorization")) {
                    auto challenges = detail::parse_challenges(*field);
                    if (challenges.size() == 1 && detail::iequal(challenges[0].scheme, "digest")) {
                        auto& c = challenges[0];
                        auto qop = c.param("qop");
                        auto alg = c.param("algorithm");
                        auto a = detail::digest_algorithm(alg ? std::string_view(*alg) : std::string_view("MD5"));
                        if (qop && *qop == "auth-int" && st->auth_int && a) {
                            return detail::digest_with_body(st, next, r, w, std::move(c), *a);
                        }
                        auto verdict = detail::check_digest(*st, c, req->method.view(), detail::request_target(*req), std::string_view());
                        if (verdict.ok) {
                            req->attach(&detail::AuthUserKey, make_tracked<string>(std::string_view(verdict.user)));
                            return next(r, w);
                        }
                        stale = verdict.stale;
                    }
                }
                detail::unauthorized(w, st->challenges(detail::auth_now_ms(), stale));
                return nullopt;
            };
        }

        tracked_ptr<detail::DigestAuthState> _s;
    };
}
