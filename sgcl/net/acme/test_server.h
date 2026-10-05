//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "client.h"
#include "error.h"
#include "types.h"
#include "detail/jws_verify.h"
#include "../http/client.h"
#include "../http/server.h"
#include "../socket.h"
#include "../tls.h"
#include "../../async/coroutine.h"
#include "../../core/map.h"
#include "../../core/ordered_map.h"
#include "../../core/set.h"
#include "../../crypto/x509.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// An ACME server on the loopback for tests (what Pebble is to Let's
// Encrypt), written from RFC 8555: the directory, nonces, accounts (External
// Account Binding, key rollover, deactivation), orders of DNS names,
// wildcards and IP addresses, authorizations and the three challenges
// validated for real (http-01 and tls-alpn-01 at the ports the options
// name, every name at the loopback; dns-01 through a lookup the test gives),
// finalization of a CSR, certificates issued by a CA of its own (a root and
// an intermediate made at the start, alternate chains under further roots
// on request), revocation, renewal information (RFC 9773), and faults on
// demand: a problem the next requests are answered with.
namespace sgcl::net::acme {
    namespace detail {
        struct TestServerState;
    }

    class test_server {
    public:
        // What the server is and how it validates
        struct options {
            string address = string("127.0.0.1:0");          // where it listens (a port of the system's choice by default)
            bool tls = false;                                 // the API over https, its certificate from its own CA (roots())
            string validation_host = string("127.0.0.1");    // where every name is validated
            uint16_t http_port = 80;                          // http-01: http://name:http_port/.well-known/acme-challenge/
            uint16_t tls_port = 443;                          // tls-alpn-01: name:tls_port, ALPN acme-tls/1
            function<async::task<expected<vector<string>, io::error>>(const string&)> lookup_txt;   // dns-01: the TXT records of a name; empty: none
            bool skip_validation = false;                     // every challenge answered is valid at once
            bool reuse_authorizations = false;                // a valid authorization of the account reused by a new order
            duration certificate_lifetime = 90 * 24 * hour;
            duration processing_time = duration::zero();      // finalize answers processing (with Retry-After) this long
            string terms_of_service;                          // non-empty: in the directory, agreeing required
            bool require_external_account = false;
            vector<external_account> external_accounts;       // the bindings taken (key id and base64url HMAC key)
            int alternate_chains = 0;                         // more chains of each certificate, under roots of their own
            bool renewal_info = true;                         // renewalInfo (RFC 9773) in the directory
            ordered_map<string, string> profiles;             // the profiles offered (name, description)
        };

        // A server listening at once, with the default options
        test_server();

        explicit test_server(const options& o);

        // Not copied: the object is the server, which stops when it is
        // destroyed (or at close()); a move hands it on
        test_server(const test_server&) = delete;
        test_server& operator=(const test_server&) = delete;

        test_server(test_server&& o) noexcept
        : _s(std::move(o._s)) {
        }

        test_server& operator=(test_server&& o) noexcept {
            if (this != &o) {
                if (_s) {
                    close();
                }
                _s = std::move(o._s);
            }
            return *this;
        }

        ~test_server() {
            if (_s) {
                close();
            }
        }

        // The URL of the directory, the client's one argument
        string directory_url() const noexcept;

        // The roots of the CA (every chain's), for verifying what it issues
        // and, with options::tls, its own certificate
        crypto::x509::certificate_pool roots() const noexcept;

        // The next `count` signed requests to `resource` (new-account,
        // new-order, order, authz, chall, finalize, cert, revoke-cert,
        // key-change; empty: any) answered with the problem of `code`, a
        // Retry-After of `retry_after` with it when not zero
        void fail_next(errc code, int count = 1, const string& resource = {}, duration retry_after = duration::zero()) const;

        // The window renewalInfo suggests for every certificate from now on
        // (by default the last third of its life)
        void set_renewal_window(const time::datetime& start, const time::datetime& end) const;

        // Whether the CA revoked the certificate
        bool revoked(const crypto::x509::certificate& cert) const noexcept;

        // The orders made so far
        size_t orders() const noexcept;

        // The certificates issued so far
        size_t certificates() const noexcept;

        // Listening stopped, the connections closed
        void close() const;

    private:
        tracked_ptr<detail::TestServerState> _s;
    };

    namespace detail {
        struct TsAccount {
            string id;
            JwkKey key;
            status st = status::valid;
            vector<string> contact;
            bool terms = false;
            vector<string> orders;
        };

        struct TsChallenge {
            string id, authz, type, token;
            status st = status::pending;
            optional<problem> error;
            optional<time::datetime> validated;
        };

        struct TsAuthz {
            string id, account;
            identifier ident;
            bool wildcard = false;
            status st = status::pending;
            time::datetime expires;
            vector<string> challenges;
        };

        struct TsOrder {
            string id, account;
            status st = status::pending;
            vector<identifier> ids;
            vector<string> authzs;
            optional<time::datetime> not_before, not_after;
            optional<problem> error;
            string cert;
            time::datetime expires;
            string replaces, profile;
            time_point ready_at;
            optional<crypto::x509::certificate_request> csr;
        };

        struct TsCert {
            string id, account;
            optional<crypto::x509::certificate> leaf;
            vector<string> chains;          // [0] the main chain, then the alternates
            bool revoked = false;
            string ari_id;
            bool replaced = false;
        };

        struct TsFault {
            errc code = errc::server_internal;
            int count = 0;
            string resource;
            duration retry_after = duration::zero();
        };

        // The CA: a root and an intermediate, the intermediate cross-signed
        // by each further root for the alternate chains; the keys unmanaged
        struct TsKeys {
            std::vector<crypto::p256::private_key> roots;
            optional<crypto::p256::private_key> intermediate;
        };

        // A problem to answer with
        struct TsProblem {
            errc code = errc::malformed;
            string detail;
            int status = 400;
            duration retry_after = duration::zero();
            string location;                 // a 409 of key-change
        };

        inline const char* problem_name(errc c) noexcept {
            for (auto& t : problem_types) {
                if (t.code == c) {
                    return t.name.data();
                }
            }
            return "serverInternal";
        }

        inline int status_of_code(errc c) noexcept {
            switch (c) {
                case errc::unauthorized:
                case errc::order_not_ready:
                case errc::user_action_required:
                    return 403;
                case errc::rate_limited: return 429;
                case errc::server_internal: return 500;
                case errc::account_does_not_exist: return 400;
                default: return 400;
            }
        }

        SGCL_INLINE_HOT TsProblem ts_problem(errc c, const string& detail) noexcept {
            return TsProblem{c, detail, status_of_code(c), duration::zero(), string()};
        }

        struct TestServerState {
            test_server::options o;
            http::server srv;
            net::listener listener;
            string base;                     // "http://127.0.0.1:port/acme"
            string origin;                   // "http://127.0.0.1:port"
            std::unique_ptr<TsKeys> keys;
            vector<crypto::x509::certificate> root_certs;
            vector<crypto::x509::certificate> intermediates;   // [0] by root 0, [i] cross-signed by root i
            crypto::x509::certificate_pool pool;
            http::client validator;          // http-01's requests, every name dialed at validation_host

            std::mutex lock;                 // the objects below but the nonces
            std::mutex nonce_lock;           // the nonces alone: answered under `lock` too
            set<string> nonces;
            map<string, TsAccount> accounts;
            map<string, TsOrder> orders;
            map<string, TsAuthz> authzs;
            map<string, TsChallenge> challenges;
            map<string, TsCert> certs;
            vector<TsFault> faults;
            optional<time::datetime> window_start, window_end;
            uint64_t next_id = 1;
            uint64_t nonce_counter = 0;
            std::atomic<size_t> order_count = 0;
            std::atomic<size_t> cert_count = 0;

            string id() noexcept {
                return string(std::to_string(next_id++));
            }

            string new_nonce() {
                std::lock_guard<std::mutex> g(nonce_lock);
                auto r = crypto::random::bytes(12);
                string n = string::concat(b64(r.as_slice()), string(std::to_string(++nonce_counter)));
                nonces.insert(n);
                return n;
            }

            string url(std::string_view resource, const string& id = {}) const {
                return id.empty() ? string::concat(base, "/", resource) : string::concat(base, "/", resource, "/", id);
            }
        };

        inline void ts_common(TestServerState& s, http::response_writer& w) {
            w.set_header(string("Replay-Nonce"), s.new_nonce());
            w.set_header(string("Link"), string::concat("<", s.url("directory"), ">;rel=\"index\""));
            w.set_header(string("Cache-Control"), string("no-store"));
        }

        inline void ts_answer_problem(TestServerState& s, http::response_writer& w, const TsProblem& p) {
            ts_common(s, w);
            w.set_header(string("Content-Type"), string("application/problem+json"));
            if (p.retry_after > duration::zero()) {
                w.set_header(string("Retry-After"), string(std::to_string(whole_seconds(p.retry_after) > 0 ? whole_seconds(p.retry_after) : 1)));
            }
            if (!p.location.empty()) {
                w.set_header(string("Location"), p.location);
            }
            w.set_status(p.status);
            w.write(json::object({{"type", string::concat(problem_prefix, problem_name(p.code))}, {"detail", p.detail}, {"status", int64_t(p.status)}}).to_string());
        }

        inline void ts_answer_json(TestServerState& s, http::response_writer& w, int status_code, const json& body, const string& location = {}) {
            ts_common(s, w);
            w.set_header(string("Content-Type"), string("application/json"));
            if (!location.empty()) {
                w.set_header(string("Location"), location);
            }
            w.set_status(status_code);
            w.write(body.to_string());
        }

        inline json ts_time(const time::datetime& t) {
            return json(time::datetime::from_unix(t.unix(), time::zone::utc()).format(time::rfc3339));
        }

        inline json ts_problem_json(const problem& p) {
            json o = json::object({{"type", p.type}, {"detail", p.detail}});
            if (p.status) {
                o = o.set(string("status"), json(int64_t(p.status)));
            }
            return o;
        }

        inline json ts_identifier_json(const identifier& i) {
            return json::object({{"type", i.type}, {"value", i.value}});
        }

        // The objects as JSON (the lock held)
        inline json ts_account_json(const TestServerState& s, const TsAccount& a) {
            json o = json::object({{"status", to_string(a.st)}, {"orders", s.url("orders", a.id)}});
            if (!a.contact.empty()) {
                o = o.set(string("contact"), strings_json(a.contact));
            }
            o = o.set(string("termsOfServiceAgreed"), json(a.terms));
            return o;
        }

        inline json ts_challenge_json(const TestServerState& s, const TsChallenge& c) {
            json o = json::object({{"type", c.type}, {"url", s.url("chall", c.id)}, {"status", to_string(c.st)}, {"token", c.token}});
            if (c.validated) {
                o = o.set(string("validated"), ts_time(*c.validated));
            }
            if (c.error) {
                o = o.set(string("error"), ts_problem_json(*c.error));
            }
            return o;
        }

        inline json ts_authz_json(const TestServerState& s, const TsAuthz& a) {
            auto list = json::array({});
            for (auto& id : a.challenges) {
                auto it = s.challenges.find(id);
                if (it != s.challenges.end()) {
                    list = list.push_back(ts_challenge_json(s, it->second));
                }
            }
            json o = json::object({{"identifier", ts_identifier_json(a.ident)}, {"status", to_string(a.st)}, {"expires", ts_time(a.expires)}, {"challenges", list}});
            if (a.wildcard) {
                o = o.set(string("wildcard"), json(true));
            }
            return o;
        }

        inline json ts_order_json(const TestServerState& s, const TsOrder& o) {
            auto ids = json::array({});
            for (auto& i : o.ids) {
                ids = ids.push_back(ts_identifier_json(i));
            }
            auto authzs = json::array({});
            for (auto& a : o.authzs) {
                authzs = authzs.push_back(json(s.url("authz", a)));
            }
            json out = json::object({{"status", to_string(o.st)}, {"expires", ts_time(o.expires)}, {"identifiers", ids}, {"authorizations", authzs},
                                     {"finalize", s.url("finalize", o.id)}});
            if (o.not_before) {
                out = out.set(string("notBefore"), ts_time(*o.not_before));
            }
            if (o.not_after) {
                out = out.set(string("notAfter"), ts_time(*o.not_after));
            }
            if (o.error) {
                out = out.set(string("error"), ts_problem_json(*o.error));
            }
            if (!o.cert.empty()) {
                out = out.set(string("certificate"), json(s.url("cert", o.cert)));
            }
            if (!o.replaces.empty()) {
                out = out.set(string("replaces"), json(o.replaces));
            }
            if (!o.profile.empty()) {
                out = out.set(string("profile"), json(o.profile));
            }
            return out;
        }

        // What a verified request is: the account (none for a jwk request),
        // the key, the payload
        struct TsRequest {
            string account;
            JwkKey key;
            string payload;
            ParsedJws jws;
        };

        enum class TsAuth : uint8_t { kid, jwk, either };

        // A signed request checked (RFC 8555 §6.2, §6.3): a fault due, the
        // content type, the JWS, the URL, the nonce (taken once), the key
        // (kid of a valid account, or jwk), the algorithm, the signature
        inline expected<TsRequest, TsProblem> ts_verify(TestServerState& s, const http::request& req, const string& body, std::string_view resource, TsAuth auth) {
            {
                std::lock_guard<std::mutex> g(s.lock);
                for (size_t i = 0; i < s.faults.size(); ++i) {
                    auto& f = s.faults[i];
                    if (f.resource.empty() || f.resource.view() == resource) {
                        TsProblem p = ts_problem(f.code, string("a fault the test asked for"));
                        p.retry_after = f.retry_after;
                        if (--f.count <= 0) {
                            s.faults.erase(s.faults.begin() + i);
                        }
                        return unexpected(p);
                    }
                }
            }
            std::string_view ct = req.header(string("Content-Type")).view();
            if (ct.substr(0, 21) != "application/jose+json") {
                TsProblem p = ts_problem(errc::malformed, string("the Content-Type is not application/jose+json"));
                p.status = 415;
                return unexpected(p);
            }
            auto jws = parse_jws(body);
            if (!jws) {
                return unexpected(ts_problem(errc::malformed, jws.error()));
            }
            string want_url = string::concat(s.origin, req.url().path());
            if (jws->url != want_url) {
                return unexpected(ts_problem(errc::unauthorized, string::concat("the JWS url is not the request's: ", jws->url)));
            }
            {
                std::lock_guard<std::mutex> g(s.nonce_lock);
                auto it = s.nonces.find(jws->nonce);
                if (jws->nonce.empty() || it == s.nonces.end()) {
                    return unexpected(ts_problem(errc::bad_nonce, string("the nonce is not one of this server's, or was used")));
                }
                s.nonces.erase(it);
            }
            TsRequest r;
            const bool has_kid = !jws->kid.empty();
            const bool has_jwk = !jws->jwk.is_null();
            if (has_kid == has_jwk) {
                return unexpected(ts_problem(errc::malformed, string("a JWS with both kid and jwk, or neither")));
            }
            if ((auth == TsAuth::kid && !has_kid) || (auth == TsAuth::jwk && !has_jwk)) {
                return unexpected(ts_problem(errc::malformed, has_kid ? string("this resource takes a jwk, not a kid") : string("this resource takes a kid, not a jwk")));
            }
            if (has_kid) {
                std::string_view kid = jws->kid.view();
                string prefix = s.url("account", string("x"));
                std::string_view p = prefix.view().substr(0, prefix.size() - 1);
                if (kid.substr(0, p.size()) != p) {
                    return unexpected(ts_problem(errc::account_does_not_exist, string("a kid that is not an account of this server")));
                }
                string id(kid.substr(p.size()));
                std::lock_guard<std::mutex> g(s.lock);
                auto it = s.accounts.find(id);
                if (it == s.accounts.end()) {
                    return unexpected(ts_problem(errc::account_does_not_exist, string("no such account")));
                }
                if (it->second.st != status::valid) {
                    return unexpected(ts_problem(errc::unauthorized, string("the account is deactivated")));
                }
                r.account = id;
                r.key = it->second.key;
            } else {
                auto k = jwk_key(jws->jwk);
                if (!k) {
                    return unexpected(ts_problem(errc::bad_public_key, string("a JWK of no kind this server takes")));
                }
                r.key = std::move(*k);
            }
            if (jws->alg != r.key.alg()) {
                return unexpected(ts_problem(errc::bad_signature_algorithm, string::concat("the alg ", jws->alg, " is not the key's")));
            }
            string input = jws->signing_input();
            if (!jws_verify(r.key, jws->alg.view(), bytes_of(input.view()), jws->signature.as_slice())) {
                return unexpected(ts_problem(errc::malformed, string("the JWS signature does not verify")));
            }
            r.payload = jws->payload;
            r.jws = std::move(*jws);
            return r;
        }

        inline bool ts_post_as_get(const TsRequest& r) noexcept {
            return r.payload.empty();
        }

        // The key authorization of a challenge for an account
        inline string ts_key_authorization(const TsChallenge& c, const TsAccount& a) {
            return string::concat(c.token, ".", a.key.thumbprint);
        }

        // An order's status after its authorizations changed (the lock held)
        inline void ts_update_order(TestServerState& s, TsOrder& o) {
            if (o.st != status::pending) {
                return;
            }
            bool all = true;
            for (auto& id : o.authzs) {
                auto& a = s.authzs[id];
                if (a.st == status::invalid || a.st == status::deactivated || a.st == status::revoked || a.st == status::expired) {
                    o.st = status::invalid;
                    o.error = problem{string::concat(problem_prefix, "unauthorized"), string::concat("the authorization of ", a.ident.value, " is ", to_string(a.st)), 403, string(), a.ident, {}};
                    return;
                }
                all &= a.st == status::valid;
            }
            if (all) {
                o.st = status::ready;
            }
        }

        // A challenge's result into it, its authorization and the orders
        // (the lock held)
        inline void ts_settle_locked(TestServerState& s, const string& chall_id, bool ok, const problem& why) {
            auto it = s.challenges.find(chall_id);
            if (it == s.challenges.end()) {
                return;
            }
            auto& c = it->second;
            auto& a = s.authzs[c.authz];
            if (ok) {
                c.st = status::valid;
                c.validated = time::now();
                a.st = status::valid;
            } else {
                c.st = status::invalid;
                c.error = why;
                a.st = status::invalid;
            }
            for (auto& [id, o] : s.orders) {
                for (auto& az : o.authzs) {
                    if (az == a.id) {
                        ts_update_order(s, o);
                        break;
                    }
                }
            }
        }

        inline void ts_settle(TestServerState& s, const string& chall_id, bool ok, const problem& why) {
            std::lock_guard<std::mutex> g(s.lock);
            ts_settle_locked(s, chall_id, ok, why);
        }

        inline problem ts_failure(const char* type, const string& detail, const identifier& id) {
            return problem{string::concat(problem_prefix, type), detail, 403, string(), id, {}};
        }

        // One challenge validated (§8.3, §8.4, RFC 8737 §3), its result
        // settled into the challenge, the authorization and the orders
        inline async::task<> ts_validate(tracked_ptr<TestServerState> s, string chall_id) noexcept {
            TsChallenge c;
            identifier ident;
            string ka;
            {
                std::lock_guard<std::mutex> g(s->lock);
                c = s->challenges[chall_id];
                auto& a = s->authzs[c.authz];
                ident = a.ident;
                ka = ts_key_authorization(c, s->accounts[a.account]);
            }
            if (s->o.skip_validation) {
                ts_settle(*s, chall_id, true, problem());
                co_return;
            }
            if (c.type == "http-01") {
                std::string host(ident.value.view());
                if (ident.type == "ip" && host.find(':') != std::string::npos) {
                    host = "[" + host + "]";
                }
                std::string url = "http://" + host + (s->o.http_port == 80 ? std::string() : ":" + std::to_string(s->o.http_port)) + "/.well-known/acme-challenge/"
                                  + std::string(c.token.view());
                auto res = co_await s->validator.async_get(string(url));
                if (!res) {
                    ts_settle(*s, chall_id, false, ts_failure("connection", res.error().message(), ident));
                    co_return;
                }
                auto body = co_await res->async_text();
                std::string_view got = body ? body->view() : std::string_view();
                while (!got.empty() && (got.back() == '\n' || got.back() == '\r' || got.back() == ' ' || got.back() == '\t')) {
                    got.remove_suffix(1);
                }
                if (res->status() != 200 || got != ka.view()) {
                    ts_settle(*s, chall_id, false, ts_failure("incorrectResponse", string::concat("the response is not the key authorization (status ", string(std::to_string(res->status())), ")"), ident));
                    co_return;
                }
                ts_settle(*s, chall_id, true, problem());
                co_return;
            }
            if (c.type == "tls-alpn-01") {
                std::string target = std::string(s->o.validation_host.view()) + ":" + std::to_string(s->o.tls_port);
                if (s->o.validation_host.view().find(':') != std::string_view::npos) {
                    target = "[" + std::string(s->o.validation_host.view()) + "]:" + std::to_string(s->o.tls_port);
                }
                auto t = co_await net::tcp::async_connect(string(target), 10 * second);
                if (!t) {
                    ts_settle(*s, chall_id, false, ts_failure("connection", t.error().message(), ident));
                    co_return;
                }
                tls::config cfg;
                if (ident.type == "ip") {
                    cfg.server_name = reverse_name(net::ip_address::parse(ident.value).value());
                } else {
                    cfg.server_name = ident.value;
                }
                cfg.alpn = {string("acme-tls/1")};
                cfg.insecure_skip_verify = true;
                auto conn = co_await tls::async_client(*t, cfg);
                if (!conn) {
                    ts_settle(*s, chall_id, false, ts_failure("tls", conn.error().message(), ident));
                    co_return;
                }
                auto st = tls::state_of(*conn);
                (void)co_await conn->async_close();
                if (!st || st->alpn != "acme-tls/1" || st->peer_certificates.empty()) {
                    ts_settle(*s, chall_id, false, ts_failure("tls", string("acme-tls/1 was not negotiated"), ident));
                    co_return;
                }
                const auto& cert = st->peer_certificates[0];
                bool names_ok;
                if (ident.type == "ip") {
                    auto ip = net::ip_address::parse(ident.value).value();
                    auto b = ip.bytes();
                    size_t from = ip.is_v4() ? 12 : 0;
                    names_ok = cert.dns_names().empty() && cert.ip_addresses().size() == 1 && cert.ip_addresses()[0].size == 16 - from;
                    for (size_t i = from; names_ok && i < 16; ++i) {
                        names_ok = cert.ip_addresses()[0].bytes[i - from] == byte(b[i]);
                    }
                } else {
                    names_ok = cert.ip_addresses().empty() && cert.dns_names().size() == 1 && cert.dns_names()[0] == ident.value;
                }
                auto h = crypto::sha256::of(bytes_of(ka.view()));
                bool ext_ok = false;
                for (auto& e : cert.extensions()) {
                    if (e.oid == "1.3.6.1.5.5.7.1.31") {
                        ext_ok = e.critical && e.value.size() == 34 && e.value[0] == byte(0x04) && e.value[1] == byte(0x20);
                        for (size_t i = 0; ext_ok && i < 32; ++i) {
                            ext_ok = e.value[2 + i] == h[i];
                        }
                    }
                }
                if (!names_ok || !ext_ok) {
                    ts_settle(*s, chall_id, false, ts_failure("incorrectResponse", string("the certificate is not for the name alone, or its acmeIdentifier is wrong"), ident));
                    co_return;
                }
                ts_settle(*s, chall_id, true, problem());
                co_return;
            }
            if (c.type == "dns-01") {
                string name = string::concat("_acme-challenge.", ident.value);
                auto h = crypto::sha256::of(bytes_of(ka.view()));
                string want = b64(bytes_of(h));
                if (!s->o.lookup_txt) {
                    ts_settle(*s, chall_id, false, ts_failure("dns", string::concat("no TXT record of ", name), ident));
                    co_return;
                }
                auto records = co_await s->o.lookup_txt(name);
                if (!records) {
                    ts_settle(*s, chall_id, false, ts_failure("dns", records.error().message(), ident));
                    co_return;
                }
                for (auto& r : *records) {
                    if (r == want) {
                        ts_settle(*s, chall_id, true, problem());
                        co_return;
                    }
                }
                ts_settle(*s, chall_id, false, ts_failure("incorrectResponse", string::concat("no TXT record of ", name, " holds the value"), ident));
                co_return;
            }
            ts_settle(*s, chall_id, false, ts_failure("malformed", string("a challenge of an unknown type"), ident));
        }

        inline string ts_pem(const crypto::x509::certificate& c) {
            return encoding::pem(string("CERTIFICATE"), vector<byte>(c.raw().begin(), c.raw().end())).to_string();
        }

        // The certificate of a finalized order issued (the lock held)
        inline void ts_issue(TestServerState& s, TsOrder& o) {
            const auto& csr = *o.csr;
            crypto::x509::certificate_template t;
            auto now = time::now();
            // backdated a minute (clocks apart), not a lifetime of seconds a test asks for
            const int64_t back = s.o.certificate_lifetime >= duration(hour) ? 60 : 0;
            time::datetime nb = o.not_before ? *o.not_before : time::datetime::from_unix(now.unix() - back, time::zone::utc());
            time::datetime na = o.not_after ? *o.not_after : nb + s.o.certificate_lifetime;
            t.not_before = nb;
            t.not_after = na;
            t.dns_names = csr.dns_names();
            t.ip_addresses = csr.ip_addresses();
            if (!csr.dns_names().empty()) {
                t.common_name = csr.dns_names()[0];
            }
            t.ext_key_usages = {crypto::x509::ext_key_usage::server_auth, crypto::x509::ext_key_usage::client_auth};
            auto leaf = crypto::x509::create_certificate(t, csr.raw_subject_public_key_info(), s.intermediates[0], *s.keys->intermediate);
            TsCert c;
            c.id = s.id();
            c.account = o.account;
            c.leaf = leaf;
            for (size_t i = 0; i < s.intermediates.size(); ++i) {
                c.chains.push_back(string::concat(ts_pem(leaf), ts_pem(s.intermediates[i])));
            }
            c.ari_id = renewal_id_of(leaf).value();
            o.cert = c.id;
            o.st = status::valid;
            s.certs[c.id] = c;
            s.cert_count.fetch_add(1);
        }

        // Whether the CSR asks for the order's identifiers, no more, no
        // fewer (RFC 8555 §7.4), by another key than the account's
        inline optional<string> ts_csr_mismatch(const TsOrder& o, const crypto::x509::certificate_request& csr, const TsAccount& a) {
            set<string> want;
            for (auto& i : o.ids) {
                want.insert(string::concat(i.type, ":", i.value));
            }
            set<string> got;
            for (auto& d : csr.dns_names()) {
                got.insert(string::concat("dns:", d));
            }
            for (auto& ip : csr.ip_addresses()) {
                net::ip_address a4;
                if (ip.size == 4) {
                    a4 = net::ip_address::v4(uint8_t(ip.bytes[0]), uint8_t(ip.bytes[1]), uint8_t(ip.bytes[2]), uint8_t(ip.bytes[3]));
                } else {
                    array<uint8_t, 16> b16 = {};
                    for (size_t i = 0; i < 16; ++i) {
                        b16[i] = uint8_t(ip.bytes[i]);
                    }
                    a4 = net::ip_address::v6(b16);
                }
                got.insert(string::concat("ip:", a4.to_string()));
            }
            string cn = csr.subject().common_name();
            if (!cn.empty() && !want.contains(string::concat("dns:", cn)) && !want.contains(string::concat("ip:", cn))) {
                return string("the CSR's common name is not one of the order's identifiers");
            }
            if (got.size() != want.size()) {
                return string("the CSR does not ask for the order's identifiers");
            }
            for (auto& g : got) {
                if (!want.contains(g)) {
                    return string("the CSR does not ask for the order's identifiers");
                }
            }
            {
                // the CSR's key compared with the account's by their SPKI
                vector<byte> account_spki;
                if (a.key.p256) {
                    account_spki = a.key.p256->to_pkix_der();
                } else if (a.key.p384) {
                    account_spki = a.key.p384->to_pkix_der();
                } else if (a.key.ed25519) {
                    account_spki = a.key.ed25519->to_pkix_der();
                } else if (a.key.rsa) {
                    account_spki = a.key.rsa->to_pkix_der();
                }
                auto spki = csr.raw_subject_public_key_info();
                if (account_spki.size() == spki.size() && std::equal(account_spki.begin(), account_spki.end(), spki.begin())) {
                    return string("the CSR's key is the account's key");
                }
            }
            return nullopt;
        }
    }
}

namespace sgcl::net::acme::detail {
    // --- the handlers ------------------------------------------------------------

    inline async::task<> ts_new_account(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "new-account", TsAuth::jwk);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        auto p = json::parse(r->payload);
        if (!p || !p->is_object()) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the payload is not a JSON object")));
            co_return;
        }
        // the account of the key, if there is one
        {
            std::lock_guard<std::mutex> g(s->lock);
            for (auto& [id, a] : s->accounts) {
                if (a.key.canonical == r->key.canonical) {
                    ts_answer_json(*s, w, 200, ts_account_json(*s, a), s->url("account", id));
                    co_return;
                }
            }
        }
        if ((*p)["onlyReturnExisting"].as_bool(false)) {
            ts_answer_problem(*s, w, ts_problem(errc::account_does_not_exist, string("no account of this key")));
            co_return;
        }
        if (!s->o.terms_of_service.empty() && !(*p)["termsOfServiceAgreed"].as_bool(false)) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the terms of service must be agreed to")));
            co_return;
        }
        TsAccount a;
        vector<string> contact;
        if (!strings_field(*p, "contact", contact, false)) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("contact is not an array of strings")));
            co_return;
        }
        for (auto& c : contact) {
            std::string_view v = c.view();
            if (v.substr(0, 7) != "mailto:") {
                ts_answer_problem(*s, w, ts_problem(errc::unsupported_contact, string::concat("only mailto: contacts: ", c)));
                co_return;
            }
            if (v.find('@') == std::string_view::npos || v.find(',') != std::string_view::npos) {
                ts_answer_problem(*s, w, ts_problem(errc::invalid_contact, string::concat("not an email address: ", c)));
                co_return;
            }
        }
        const json& eab_json = (*p)["externalAccountBinding"];
        if (s->o.require_external_account && eab_json.is_null()) {
            ts_answer_problem(*s, w, ts_problem(errc::external_account_required, string("an external account binding is required")));
            co_return;
        }
        if (!eab_json.is_null()) {
            auto inner = parse_jws(eab_json.to_string());
            if (!inner || inner->alg != "HS256" || inner->url != r->jws.url || !inner->nonce.empty() || inner->kid.empty()) {
                ts_answer_problem(*s, w, ts_problem(errc::malformed, string("an external account binding that is not a JWS of HS256 with a kid and the newAccount URL, without a nonce")));
                co_return;
            }
            optional<vector<byte>> mac;
            for (auto& e : s->o.external_accounts) {
                if (e.key_id == inner->kid) {
                    mac = b64_decode(e.hmac_key);
                }
            }
            if (!mac) {
                ts_answer_problem(*s, w, ts_problem(errc::unauthorized, string("an external account binding of an unknown key id")));
                co_return;
            }
            string input = inner->signing_input();
            auto tag = crypto::hmac<crypto::sha256>::of(bytes_of(input.view()), mac->as_slice());
            if (!crypto::constant_time::equal(tag, inner->signature.as_slice())) {
                ts_answer_problem(*s, w, ts_problem(errc::unauthorized, string("the external account binding's MAC does not verify")));
                co_return;
            }
            auto bound = json::parse(inner->payload);
            auto bound_key = bound ? jwk_key(*bound) : nullopt;
            if (!bound_key || bound_key->canonical != r->key.canonical) {
                ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the external account binding is of another key")));
                co_return;
            }
        }
        a.key = r->key;
        a.contact = contact;
        a.terms = (*p)["termsOfServiceAgreed"].as_bool(false);
        string id;
        json out;
        {
            std::lock_guard<std::mutex> g(s->lock);
            a.id = id = s->id();
            s->accounts[id] = a;
            out = ts_account_json(*s, a);
        }
        ts_answer_json(*s, w, 201, out, s->url("account", id));
    }

    inline async::task<> ts_account(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "account", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        if (req.path_value(string("id")) != r->account) {
            ts_answer_problem(*s, w, ts_problem(errc::unauthorized, string("the account of the URL is not the signer's")));
            co_return;
        }
        json p = json::object({});
        if (!ts_post_as_get(*r)) {
            auto pj = json::parse(r->payload);
            if (!pj || !pj->is_object()) {
                ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the payload is not a JSON object")));
                co_return;
            }
            p = *pj;
        }
        std::lock_guard<std::mutex> g(s->lock);
        auto& a = s->accounts[r->account];
        if (const json& c = p["contact"]; !c.is_null()) {
            vector<string> contact;
            if (!strings_field(p, "contact", contact, false)) {
                ts_answer_problem(*s, w, ts_problem(errc::malformed, string("contact is not an array of strings")));
                co_return;
            }
            a.contact = contact;
        }
        if (auto st = p["status"].as_string()) {
            if (*st != "deactivated") {
                ts_answer_problem(*s, w, ts_problem(errc::malformed, string("an account's status may only become deactivated")));
                co_return;
            }
            a.st = status::deactivated;
        }
        ts_answer_json(*s, w, 200, ts_account_json(*s, a));
    }

    inline async::task<> ts_new_order(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "new-order", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        auto p = json::parse(r->payload);
        if (!p || !p->is_object() || !(*p)["identifiers"].is_array() || (*p)["identifiers"].size() == 0) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("an order without identifiers")));
            co_return;
        }
        TsOrder o;
        for (const json& i : (*p)["identifiers"].elements()) {
            identifier id;
            if (!identifier_of(i, id)) {
                ts_answer_problem(*s, w, ts_problem(errc::malformed, string("a malformed identifier")));
                co_return;
            }
            if (id.type == "ip") {
                auto a = net::ip_address::parse(id.value);
                if (!a || a->to_string() != id.value) {
                    ts_answer_problem(*s, w, ts_problem(errc::rejected_identifier, string::concat("not an IP address in its canonical text: ", id.value)));
                    co_return;
                }
            } else if (id.type == "dns") {
                std::string_view v = id.value.view();
                if (v.empty() || (v.find("*") != std::string_view::npos && (v.substr(0, 2) != "*." || v.find('*', 1) != std::string_view::npos))) {
                    ts_answer_problem(*s, w, ts_problem(errc::rejected_identifier, string::concat("not a DNS name: ", id.value)));
                    co_return;
                }
            } else {
                ts_answer_problem(*s, w, ts_problem(errc::unsupported_identifier, string::concat("an identifier of type ", id.type)));
                co_return;
            }
            o.ids.push_back(id);
        }
        optional<time::datetime> nb, na;
        if (!time_field(*p, "notBefore", nb) || !time_field(*p, "notAfter", na)) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("notBefore or notAfter that is not RFC 3339")));
            co_return;
        }
        o.not_before = nb;
        o.not_after = na;
        o.replaces = (*p)["replaces"].as_string(string());
        o.profile = (*p)["profile"].as_string(string());
        if (!o.profile.empty() && !s->o.profiles.contains(o.profile)) {
            ts_answer_problem(*s, w, ts_problem(errc::invalid_profile, string::concat("no profile ", o.profile)));
            co_return;
        }
        json out;
        string id;
        {
            std::lock_guard<std::mutex> g(s->lock);
            if (!o.replaces.empty()) {
                TsCert* found = nullptr;
                for (auto& [cid, c] : s->certs) {
                    if (c.ari_id == o.replaces) {
                        found = &c;
                    }
                }
                if (!found || found->account != r->account) {
                    ts_answer_problem(*s, w, ts_problem(errc::malformed, string("replaces: no certificate of this account has that id")));
                    co_return;
                }
                if (found->replaced) {
                    ts_answer_problem(*s, w, ts_problem(errc::already_replaced, string("the certificate was already replaced")));
                    co_return;
                }
                found->replaced = true;
            }
            o.id = id = s->id();
            o.account = r->account;
            o.expires = time::now() + 7 * 24 * hour;
            for (auto& i : o.ids) {
                bool wildcard = i.type == "dns" && i.value.view().substr(0, 2) == "*.";
                identifier bare = i;
                if (wildcard) {
                    bare.value = string(i.value.view().substr(2));
                }
                if (s->o.reuse_authorizations) {
                    string reused;
                    for (auto& [aid, a] : s->authzs) {
                        if (a.account == r->account && a.st == status::valid && a.ident == bare && a.wildcard == wildcard) {
                            reused = aid;
                        }
                    }
                    if (!reused.empty()) {
                        o.authzs.push_back(reused);
                        continue;
                    }
                }
                TsAuthz a;
                a.id = s->id();
                a.account = r->account;
                a.ident = bare;
                a.wildcard = wildcard;
                a.expires = time::now() + 7 * 24 * hour;
                // a wildcard by dns-01 alone (§7.1.3), an address by http-01 and
                // tls-alpn-01 alone (RFC 8738 §7)
                vector<const char*> types;
                if (wildcard) {
                    types = {"dns-01"};
                } else if (i.type == "ip") {
                    types = {"http-01", "tls-alpn-01"};
                } else {
                    types = {"http-01", "dns-01", "tls-alpn-01"};
                }
                for (auto t : types) {
                    TsChallenge c;
                    c.id = s->id();
                    c.authz = a.id;
                    c.type = string(t);
                    c.token = b64(crypto::random::bytes(32).as_slice());
                    a.challenges.push_back(c.id);
                    s->challenges[c.id] = c;
                }
                s->authzs[a.id] = a;
                o.authzs.push_back(a.id);
            }
            ts_update_order(*s, o);
            s->orders[id] = o;
            s->accounts[r->account].orders.push_back(id);
            s->order_count.fetch_add(1);
            out = ts_order_json(*s, o);
        }
        ts_answer_json(*s, w, 201, out, s->url("order", id));
    }

    inline async::task<> ts_order(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "order", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        std::lock_guard<std::mutex> g(s->lock);
        auto it = s->orders.find(req.path_value(string("id")));
        if (it == s->orders.end() || it->second.account != r->account) {
            ts_answer_problem(*s, w, TsProblem{errc::malformed, string("no such order"), 404, duration::zero(), string()});
            co_return;
        }
        auto& o = it->second;
        if (o.st == status::processing && sgcl::clock::now() >= o.ready_at) {
            ts_issue(*s, o);
        }
        if (o.st == status::processing) {
            w.set_header(string("Retry-After"), string("1"));
        }
        ts_answer_json(*s, w, 200, ts_order_json(*s, o));
    }

    inline async::task<> ts_authz(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "authz", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        std::lock_guard<std::mutex> g(s->lock);
        auto it = s->authzs.find(req.path_value(string("id")));
        if (it == s->authzs.end() || it->second.account != r->account) {
            ts_answer_problem(*s, w, TsProblem{errc::malformed, string("no such authorization"), 404, duration::zero(), string()});
            co_return;
        }
        auto& a = it->second;
        if (!ts_post_as_get(*r)) {
            auto p = json::parse(r->payload);
            if (!p || p->operator[]("status").as_string(string()) != "deactivated") {
                ts_answer_problem(*s, w, ts_problem(errc::malformed, string("an authorization's status may only become deactivated")));
                co_return;
            }
            if (a.st == status::pending || a.st == status::valid) {
                a.st = status::deactivated;
                for (auto& [id, o] : s->orders) {
                    for (auto& az : o.authzs) {
                        if (az == a.id) {
                            ts_update_order(*s, o);
                        }
                    }
                }
            }
        }
        if (a.st == status::pending) {
            w.set_header(string("Retry-After"), string("1"));
        }
        ts_answer_json(*s, w, 200, ts_authz_json(*s, a));
    }

    inline async::task<> ts_chall(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "chall", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        string id = req.path_value(string("id"));
        bool start = false;
        json out;
        {
            std::lock_guard<std::mutex> g(s->lock);
            auto it = s->challenges.find(id);
            if (it == s->challenges.end() || s->authzs[it->second.authz].account != r->account) {
                ts_answer_problem(*s, w, TsProblem{errc::malformed, string("no such challenge"), 404, duration::zero(), string()});
                co_return;
            }
            auto& c = it->second;
            if (!ts_post_as_get(*r)) {
                auto p = json::parse(r->payload);
                if (!p || !p->is_object()) {
                    ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the payload of a challenge's answer is not an object")));
                    co_return;
                }
                auto& a = s->authzs[c.authz];
                if (c.st == status::pending && a.st == status::pending) {
                    if (s->o.skip_validation) {
                        ts_settle_locked(*s, id, true, problem());   // valid in the answer itself
                    } else {
                        c.st = status::processing;
                        start = true;
                    }
                }
            }
            out = ts_challenge_json(*s, c);
            w.set_header(string("Link"), string::concat("<", s->url("authz", c.authz), ">;rel=\"up\""));
        }
        if (start) {
            async::go(ts_validate(s, id));
        }
        ts_answer_json(*s, w, 200, out);
    }

    inline async::task<> ts_finalize(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "finalize", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        auto p = json::parse(r->payload);
        auto csr_text = p ? (*p)["csr"].as_string() : nullopt;
        auto der = csr_text ? b64_decode(*csr_text) : nullopt;
        if (!der) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("a finalize without a csr in base64url")));
            co_return;
        }
        auto csr = crypto::x509::certificate_request::parse(der->as_slice());
        if (!csr || !csr->check_signature()) {
            ts_answer_problem(*s, w, ts_problem(errc::bad_csr, string("the CSR does not parse or its signature does not verify")));
            co_return;
        }
        std::lock_guard<std::mutex> g(s->lock);
        auto it = s->orders.find(req.path_value(string("id")));
        if (it == s->orders.end() || it->second.account != r->account) {
            ts_answer_problem(*s, w, TsProblem{errc::malformed, string("no such order"), 404, duration::zero(), string()});
            co_return;
        }
        auto& o = it->second;
        if (o.st != status::ready) {
            ts_answer_problem(*s, w, ts_problem(errc::order_not_ready, string::concat("the order is ", to_string(o.st))));
            co_return;
        }
        if (auto why = ts_csr_mismatch(o, *csr, s->accounts[r->account])) {
            ts_answer_problem(*s, w, ts_problem(errc::bad_csr, *why));
            co_return;
        }
        o.csr = *csr;
        if (s->o.processing_time > duration::zero()) {
            o.st = status::processing;
            o.ready_at = sgcl::clock::now() + s->o.processing_time;
            w.set_header(string("Retry-After"), string("1"));
        } else {
            ts_issue(*s, o);
        }
        ts_answer_json(*s, w, 200, ts_order_json(*s, o), s->url("order", o.id));
    }

    inline async::task<> ts_cert(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "cert", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        string id = req.path_value(string("id"));
        string alt = req.path_value(string("alt"));
        std::lock_guard<std::mutex> g(s->lock);
        auto it = s->certs.find(id);
        if (it == s->certs.end() || it->second.account != r->account) {
            ts_answer_problem(*s, w, TsProblem{errc::malformed, string("no such certificate"), 404, duration::zero(), string()});
            co_return;
        }
        auto& c = it->second;
        size_t n = 0;
        if (!alt.empty()) {
            n = size_t(std::atoi(std::string(alt.view()).c_str()));
            if (n == 0 || n >= c.chains.size()) {
                ts_answer_problem(*s, w, TsProblem{errc::malformed, string("no such chain"), 404, duration::zero(), string()});
                co_return;
            }
        }
        ts_common(*s, w);
        for (size_t i = 0; i < c.chains.size(); ++i) {
            if (i == n) {
                continue;
            }
            string u = i == 0 ? s->url("cert", id) : string::concat(s->url("cert", id), "/", string(std::to_string(i)));
            w.add_header(string("Link"), string::concat("<", u, ">;rel=\"alternate\""));
        }
        w.set_header(string("Content-Type"), string("application/pem-certificate-chain"));
        w.write(c.chains[n]);
    }

    inline async::task<> ts_revoke(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "revoke-cert", TsAuth::either);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        auto p = json::parse(r->payload);
        auto text = p ? (*p)["certificate"].as_string() : nullopt;
        auto der = text ? b64_decode(*text) : nullopt;
        if (!der) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("a revocation without a certificate in base64url")));
            co_return;
        }
        int64_t reason = (*p)["reason"].as_int(0);
        if (reason < 0 || reason > 10 || reason == 7) {
            ts_answer_problem(*s, w, ts_problem(errc::bad_revocation_reason, string("a reason RFC 5280 §5.3.1 does not have")));
            co_return;
        }
        std::lock_guard<std::mutex> g(s->lock);
        TsCert* found = nullptr;
        for (auto& [id, c] : s->certs) {
            auto raw = c.leaf->raw();
            if (raw.size() == der->size() && std::equal(raw.begin(), raw.end(), der->begin())) {
                found = &c;
            }
        }
        if (!found) {
            ts_answer_problem(*s, w, TsProblem{errc::malformed, string("no certificate of this CA"), 404, duration::zero(), string()});
            co_return;
        }
        bool allowed = !r->account.empty() ? found->account == r->account : false;
        if (r->account.empty()) {
            // by the certificate's key: the JWK's key is the leaf's
            const auto& pk = found->leaf->public_key();
            if (pk.kind() == crypto::x509::key_kind::p256 && r->key.p256) {
                allowed = pk.p256() == *r->key.p256;
            } else if (pk.kind() == crypto::x509::key_kind::p384 && r->key.p384) {
                allowed = pk.p384() == *r->key.p384;
            } else if (pk.kind() == crypto::x509::key_kind::ed25519 && r->key.ed25519) {
                allowed = pk.ed25519() == *r->key.ed25519;
            } else if (pk.kind() == crypto::x509::key_kind::rsa && r->key.rsa) {
                auto x = pk.rsa().to_pkix_der();
                auto y = r->key.rsa->to_pkix_der();
                allowed = x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin());
            }
        }
        if (!allowed) {
            ts_answer_problem(*s, w, ts_problem(errc::unauthorized, string("neither the certificate's account nor its key")));
            co_return;
        }
        if (found->revoked) {
            ts_answer_problem(*s, w, ts_problem(errc::already_revoked, string("the certificate is already revoked")));
            co_return;
        }
        found->revoked = true;
        ts_common(*s, w);
        w.set_status(200);
    }

    inline async::task<> ts_key_change(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        auto body = co_await req.async_text();
        auto r = ts_verify(*s, req, body ? *body : string(), "key-change", TsAuth::kid);
        if (!r) {
            ts_answer_problem(*s, w, r.error());
            co_return;
        }
        auto inner = parse_jws(r->payload);
        if (!inner || inner->jwk.is_null() || !inner->kid.empty() || !inner->nonce.empty() || inner->url != r->jws.url) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the inner JWS has no jwk, or a kid or a nonce, or another url")));
            co_return;
        }
        auto next = jwk_key(inner->jwk);
        if (!next) {
            ts_answer_problem(*s, w, ts_problem(errc::bad_public_key, string("a new key of no kind this server takes")));
            co_return;
        }
        string input = inner->signing_input();
        if (inner->alg != next->alg() || !jws_verify(*next, inner->alg.view(), bytes_of(input.view()), inner->signature.as_slice())) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the inner JWS does not verify under the new key")));
            co_return;
        }
        auto p = json::parse(inner->payload);
        auto old = p ? jwk_key((*p)["oldKey"]) : nullopt;
        if (!p || (*p)["account"].as_string(string()) != r->jws.kid || !old || old->canonical != r->key.canonical) {
            ts_answer_problem(*s, w, ts_problem(errc::malformed, string("the inner payload's account or oldKey is not the signer's")));
            co_return;
        }
        std::lock_guard<std::mutex> g(s->lock);
        for (auto& [id, a] : s->accounts) {
            if (a.key.canonical == next->canonical) {
                TsProblem c = ts_problem(errc::malformed, string("the new key is another account's"));
                c.status = 409;
                c.location = s->url("account", id);
                ts_answer_problem(*s, w, c);
                co_return;
            }
        }
        auto& a = s->accounts[r->account];
        a.key = std::move(*next);
        ts_answer_json(*s, w, 200, ts_account_json(*s, a));
    }

    inline void ts_renewal_info(tracked_ptr<TestServerState> s, http::request req, http::response_writer w) {
        string id = req.path_value(string("id"));
        std::lock_guard<std::mutex> g(s->lock);
        for (auto& [cid, c] : s->certs) {
            if (c.ari_id == id) {
                time::datetime start, end;
                if (s->window_start) {
                    start = *s->window_start;
                    end = *s->window_end;
                } else {
                    int64_t nb = c.leaf->not_before().unix(), na = c.leaf->not_after().unix();
                    int64_t life = na - nb;
                    start = time::datetime::from_unix(nb + life * 2 / 3, time::zone::utc());
                    end = time::datetime::from_unix(nb + life * 2 / 3 + life / 9, time::zone::utc());
                }
                w.set_header(string("Retry-After"), string("21600"));
                w.set_header(string("Content-Type"), string("application/json"));
                w.write(json::object({{"suggestedWindow", json::object({{"start", ts_time(start)}, {"end", ts_time(end)}})}}).to_string());
                return;
            }
        }
        w.set_status(404);
        w.set_header(string("Content-Type"), string("application/problem+json"));
        w.write(json::object({{"type", string::concat(problem_prefix, "malformed")}, {"detail", "no certificate of that id"}, {"status", int64_t(404)}}).to_string());
    }

    inline void ts_directory(tracked_ptr<TestServerState> s, http::response_writer w) {
        json meta = json::object({{"externalAccountRequired", s->o.require_external_account}});
        if (!s->o.terms_of_service.empty()) {
            meta = meta.set(string("termsOfService"), json(s->o.terms_of_service));
        }
        if (!s->o.profiles.empty()) {
            json p = json::object({});
            for (auto& [k, v] : s->o.profiles) {
                p = p.set(k, json(v));
            }
            meta = meta.set(string("profiles"), p);
        }
        json d = json::object({{"newNonce", s->url("new-nonce")}, {"newAccount", s->url("new-account")}, {"newOrder", s->url("new-order")},
                               {"revokeCert", s->url("revoke-cert")}, {"keyChange", s->url("key-change")}, {"meta", meta}});
        if (s->o.renewal_info) {
            d = d.set(string("renewalInfo"), json(s->url("renewal-info")));
        }
        w.set_header(string("Content-Type"), string("application/json"));
        w.write(d.to_string());
    }
}

namespace sgcl::net::acme {
    inline test_server::test_server()
    : test_server(options()) {
    }

    inline test_server::test_server(const options& o)
    : _s(make_tracked<detail::TestServerState>()) {
        using namespace detail;
        auto& s = *_s;
        s.o = o;
        s.keys = std::make_unique<TsKeys>();
        // the CA: a root, an intermediate it signs, and further roots that
        // cross-sign the intermediate's key for the alternate chains
        auto now = time::now();
        const int roots = 1 + (o.alternate_chains > 0 ? o.alternate_chains : 0);
        s.keys->intermediate.emplace(crypto::p256::private_key::generate());
        auto inter_spki = s.keys->intermediate->public_key().to_pkix_der();
        for (int i = 0; i < roots; ++i) {
            s.keys->roots.push_back(crypto::p256::private_key::generate());
            crypto::x509::certificate_template t;
            t.common_name = string::concat("sgcl acme test root ", string(std::to_string(i)));
            t.is_ca = true;
            t.not_before = now - 24 * hour;
            t.not_after = now + 3650 * 24 * hour;
            auto root = crypto::x509::create_certificate(t, s.keys->roots.back());
            s.root_certs.push_back(root);
            s.pool.add(root);
            crypto::x509::certificate_template it;
            it.common_name = string("sgcl acme test intermediate");
            it.is_ca = true;
            it.max_path_length = 0;
            it.not_before = now - 24 * hour;
            it.not_after = now + 1825 * 24 * hour;
            s.intermediates.push_back(crypto::x509::create_certificate(it, inter_spki.as_slice(), root, s.keys->roots.back()));
        }
        // every name validated at validation_host
        string host = o.validation_host;
        s.validator.dial = [host](const net::url& u, async::stop_token) -> async::task<expected<net::connection, io::error>> {
            string port = string(std::to_string(u.effective_port()));
            string target = host.view().find(':') != std::string_view::npos ? string::concat("[", host, "]:", port) : string::concat(host, ":", port);
            co_return co_await net::tcp::async_connect(target, 10 * second);
        };
        s.validator.timeout = 10 * second;
        s.validator.proxy = http::proxy();   // the names at validation_host, never through the environment's proxy
        auto l = net::tcp::listen(o.address);
        if (!l) {
            throw std::runtime_error(std::string("sgcl::net::acme::test_server: ") + std::string(l.error().message().view()));
        }
        auto ep = l->local_endpoint();
        std::string addr = ep.address().is_v6() ? "[" + std::string(ep.address().to_string().view()) + "]" : std::string(ep.address().to_string().view());
        s.origin = string::concat(o.tls ? "https://" : "http://", string(addr), ":", string(std::to_string(ep.port())));
        s.base = string::concat(s.origin, "/acme");
        net::listener listener = *l;
        if (o.tls) {
            // its own certificate for the loopback, by the intermediate
            auto key = crypto::p256::private_key::generate();
            crypto::x509::certificate_template t;
            t.dns_names = {string("localhost")};
            crypto::x509::ip_address v4;
            v4.size = 4;
            v4.bytes[0] = byte(127);
            v4.bytes[3] = byte(1);
            crypto::x509::ip_address v6;
            v6.size = 16;
            v6.bytes[15] = byte(1);
            t.ip_addresses = {v4, v6};
            t.ext_key_usages = {crypto::x509::ext_key_usage::server_auth};
            auto spki = key.public_key().to_pkix_der();
            auto leaf = crypto::x509::create_certificate(t, spki.as_slice(), s.intermediates[0], *s.keys->intermediate);
            string chain = string::concat(ts_pem(leaf), ts_pem(s.intermediates[0]));
            auto key_pem = key.to_pem();
            tls::config cfg;
            cfg.identities = {tls::identity(chain, key_pem.as_slice())};
            cfg.alpn = {string("http/1.1")};
            listener = tls::detail::make_listener(*l, cfg).value();
        }
        s.listener = listener;
        tracked_ptr<TestServerState> st = _s;
        s.srv.route("GET /acme/directory", [st](http::request, http::response_writer w) { ts_directory(st, w); });
        s.srv.route("HEAD /acme/new-nonce", [st](http::request, http::response_writer w) {
            w.set_header(string("Replay-Nonce"), st->new_nonce());
            w.set_header(string("Cache-Control"), string("no-store"));
            w.set_status(200);
        });
        s.srv.route("GET /acme/new-nonce", [st](http::request, http::response_writer w) {
            w.set_header(string("Replay-Nonce"), st->new_nonce());
            w.set_header(string("Cache-Control"), string("no-store"));
            w.set_status(204);
        });
        s.srv.route("POST /acme/new-account", [st](http::request r, http::response_writer w) { return ts_new_account(st, r, w); });
        s.srv.route("POST /acme/account/{id}", [st](http::request r, http::response_writer w) { return ts_account(st, r, w); });
        s.srv.route("POST /acme/new-order", [st](http::request r, http::response_writer w) { return ts_new_order(st, r, w); });
        s.srv.route("POST /acme/order/{id}", [st](http::request r, http::response_writer w) { return ts_order(st, r, w); });
        s.srv.route("POST /acme/authz/{id}", [st](http::request r, http::response_writer w) { return ts_authz(st, r, w); });
        s.srv.route("POST /acme/chall/{id}", [st](http::request r, http::response_writer w) { return ts_chall(st, r, w); });
        s.srv.route("POST /acme/finalize/{id}", [st](http::request r, http::response_writer w) { return ts_finalize(st, r, w); });
        s.srv.route("POST /acme/cert/{id}", [st](http::request r, http::response_writer w) { return ts_cert(st, r, w); });
        s.srv.route("POST /acme/cert/{id}/{alt}", [st](http::request r, http::response_writer w) { return ts_cert(st, r, w); });
        s.srv.route("POST /acme/revoke-cert", [st](http::request r, http::response_writer w) { return ts_revoke(st, r, w); });
        s.srv.route("POST /acme/key-change", [st](http::request r, http::response_writer w) { return ts_key_change(st, r, w); });
        s.srv.route("GET /acme/renewal-info/{id}", [st](http::request r, http::response_writer w) { ts_renewal_info(st, r, w); });
        s.srv.on_error = [](const string&) {};
        async::go([](http::server srv, net::listener l) -> async::task<> { (void)co_await srv.async_serve(l); }(s.srv, listener));
    }

    inline string test_server::directory_url() const noexcept {
        return _s->url("directory");
    }

    inline crypto::x509::certificate_pool test_server::roots() const noexcept {
        return _s->pool;
    }

    inline void test_server::fail_next(errc code, int count, const string& resource, duration retry_after) const {
        std::lock_guard<std::mutex> g(_s->lock);
        if (count > 0) {
            _s->faults.push_back(detail::TsFault{code, count, resource, retry_after});
        }
    }

    inline void test_server::set_renewal_window(const time::datetime& start, const time::datetime& end) const {
        std::lock_guard<std::mutex> g(_s->lock);
        _s->window_start = start;
        _s->window_end = end;
    }

    inline bool test_server::revoked(const crypto::x509::certificate& cert) const noexcept {
        std::lock_guard<std::mutex> g(_s->lock);
        for (auto& [id, c] : _s->certs) {
            if (*c.leaf == cert) {
                return c.revoked;
            }
        }
        return false;
    }

    inline size_t test_server::orders() const noexcept {
        return _s->order_count.load();
    }

    inline size_t test_server::certificates() const noexcept {
        return _s->cert_count.load();
    }

    inline void test_server::close() const {
        _s->srv.close();
    }
}
