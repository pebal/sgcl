//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../oauth2/oauth2.h"
#include "../../crypto/jose.h"
#include "../../crypto/random.h"
#include "../../async/mutex.h"
#include "../../core/aliases.h"
#include "../../core/atomic.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/base64.h"
#include "../../encoding/json.h"
#include "../../time/datetime.h"

#include <chrono>
#include <cstdint>
#include <string_view>

// OpenID Connect on net::oauth2 (Core 1.0, Discovery 1.0): a provider found
// by its issuer, its keys (jwks_uri) fetched when first needed and again
// when a token names a key the set lacks; ID tokens verified with
// crypto::jose (the signature by the provider's keys, iss, aud and azp,
// exp, iat, nonce, Core §3.1.3.7); the userinfo endpoint. Go's
// coreos/go-oidc in shape. Its own namespace, sgcl::net::oidc.
namespace sgcl::net::oidc {
    // An ID token verified (Core §2): the claims a client decides by, and
    // all of them
    struct id_token {
        string issuer;
        string subject;
        vector<string> audience;
        time::datetime expiry;
        time::datetime issued_at;
        string nonce;
        encoding::json claims;
    };

    // The state and the nonce of one authorization request: 128 random bits
    // each, base64url; the state ties the redirect to the session (CSRF),
    // the nonce the ID token to the request (replay)
    struct request_secrets {
        string state;
        string nonce;

        static request_secrets generate() {
            auto one = [] {
                byte raw[16];
                crypto::random::fill(slice<byte>(raw, sizeof raw));
                return encoding::base64::raw_url.encode(slice<const byte>(raw, sizeof raw));
            };
            request_secrets out;
            out.state = one();
            out.nonce = one();
            return out;
        }
    };

    class provider;

    namespace detail {
        using namespace sgcl::net::http::detail;

        // A key set as it was fetched, never changed: a new fetch publishes a new one
        struct KeySet {
            crypto::jose::jwk_set set;
        };

        // What a provider keeps: its metadata, its keys and when they came
        struct ProviderState {
            string issuer;
            encoding::json metadata;
            oauth2::endpoints endpoints;
            string userinfo;
            string jwks_uri;
            http::client http;
            async::mutex lock;                   // one fetch of the keys at a time
            sgcl::atomic<tracked_ptr<KeySet>> keys;   // the last set fetched, read without the lock
            std::atomic<int64_t> fetched_ms{0};  // when the last fetch was tried; 0: never
        };

        inline oauth2::error invalid(std::string_view code, std::string_view description) {
            return oauth2::error(string(code), string(description));
        }

        // A GET of a JSON object (the discovery document, the key set, userinfo)
        inline async::task<expected<encoding::json, oauth2::error>> get_json(http::client c, http::request req) noexcept {
            req.set_header("Accept", "application/json");
            auto res = co_await c.async_send(req);
            if (!res) {
                co_return unexpected(oauth2::error(res.error()));
            }
            co_return co_await oauth2::detail::read_json(*res, false);
        }

        struct ProviderAccess;
    }

    // An OpenID provider (Discovery 1.0): its metadata read from
    // issuer + "/.well-known/openid-configuration", the issuer it names held
    // to the one asked for (§4.3). A handle of one word: copies share the
    // metadata and the keys
    class provider {
    public:
        // How an ID token is checked
        struct verify_options {
            string client_id;                                   // the audience: aud must hold it
            string nonce;                                       // the request's nonce; "" not checked
            duration leeway = std::chrono::minutes(1);          // the clocks' skew allowed for exp and iat
        };

        static expected<provider, oauth2::error> discover(const string& issuer) {
            return async_discover(issuer, http::client()).wait();
        }

        static expected<provider, oauth2::error> discover(const string& issuer, const http::client& c) {
            return async_discover(issuer, c).wait();
        }

        static async::task<expected<provider, oauth2::error>> async_discover(string issuer) noexcept {
            return async_discover(std::move(issuer), http::client());
        }

        static async::task<expected<provider, oauth2::error>> async_discover(string issuer, http::client c) noexcept {
            return _co_discover(std::move(issuer), std::move(c));
        }

        SGCL_INLINE_HOT string issuer() const noexcept {
            return _s->issuer;
        }

        // The whole discovery document, for the provider's own members
        SGCL_INLINE_HOT encoding::json metadata() const noexcept {
            return _s->metadata;
        }

        // The endpoints of the metadata, for an oauth2::config
        SGCL_INLINE_HOT oauth2::endpoints endpoints() const noexcept {
            return _s->endpoints;
        }

        // An oauth2::config of the provider's endpoints with the client's id,
        // secret and redirect, the scope "openid" first
        oauth2::config config(const string& client_id, const string& client_secret, const string& redirect_url,
                              const vector<string>& scopes = {}) const {
            oauth2::config c;
            c.client_id = client_id;
            c.client_secret = client_secret;
            c.redirect_url = redirect_url;
            c.endpoints = _s->endpoints;
            c.http = _s->http;
            c.scopes.push_back(string("openid"));
            for (auto& s : scopes) {
                if (s != "openid") {
                    c.scopes.push_back(s);
                }
            }
            return c;
        }

        // An ID token checked (Core §3.1.3.7): its signature by a key of the
        // provider's set (fetched again, at most once a minute, when the token
        // names a kid the set lacks), iss the provider's, aud holding the
        // client (and azp the client when there are several audiences, or
        // when it is there), exp not past and iat present, the nonce the
        // request's
        expected<id_token, oauth2::error> verify(const string& raw, const verify_options& o) const {
            return async_verify(raw, o).wait();
        }

        async::task<expected<id_token, oauth2::error>> async_verify(string raw, verify_options o) const noexcept {
            return _co_verify(_s, std::move(raw), std::move(o));
        }

        // The claims of the userinfo endpoint (Core §5.3) for the token's
        // access token; its sub held to the subject given (the ID token's)
        expected<encoding::json, oauth2::error> userinfo(const oauth2::token& t, const string& subject = {}) const {
            return async_userinfo(t, subject).wait();
        }

        async::task<expected<encoding::json, oauth2::error>> async_userinfo(oauth2::token t, string subject = {}) const noexcept {
            return _co_userinfo(_s, std::move(t), std::move(subject));
        }

    private:
        friend struct detail::ProviderAccess;

        explicit provider(tracked_ptr<detail::ProviderState> s) noexcept
        : _s(std::move(s)) {
        }

        static async::task<expected<provider, oauth2::error>> _co_discover(string issuer, http::client c) noexcept {
            std::string_view base = issuer.view();
            while (!base.empty() && base.back() == '/') {
                base.remove_suffix(1);
            }
            auto v = co_await detail::get_json(c, http::request("GET", string::concat(base, "/.well-known/openid-configuration")));
            if (!v) {
                co_return unexpected(v.error());
            }
            const encoding::json& m = *v;
            if (m["issuer"].as_string("") != issuer) {
                co_return unexpected(detail::invalid("invalid_issuer", "the discovery document names another issuer"));
            }
            tracked_ptr s = make_tracked<detail::ProviderState>();
            s->issuer = issuer;
            s->metadata = m;
            s->http = c;
            s->endpoints.authorization = m["authorization_endpoint"].as_string("");
            s->endpoints.token = m["token_endpoint"].as_string("");
            s->endpoints.device_authorization = m["device_authorization_endpoint"].as_string("");
            s->endpoints.revocation = m["revocation_endpoint"].as_string("");
            s->endpoints.introspection = m["introspection_endpoint"].as_string("");
            s->userinfo = m["userinfo_endpoint"].as_string("");
            s->jwks_uri = m["jwks_uri"].as_string("");
            if (s->jwks_uri.empty()) {
                co_return unexpected(detail::invalid("invalid_metadata", "the discovery document has no jwks_uri"));
            }
            co_return provider(s);
        }

        // The key set fetched (under the lock); kept when the fetch fails
        static async::task<expected<void, oauth2::error>> _co_fetch_keys(tracked_ptr<detail::ProviderState> s) noexcept {
            http::request req("GET", s->jwks_uri);
            req.set_header("Accept", "application/json");
            auto res = co_await s->http.async_send(req);
            s->fetched_ms = time::now().unix_milli();
            if (!res) {
                co_return unexpected(oauth2::error(res.error()));
            }
            auto text = co_await res->async_text();
            if (!text) {
                co_return unexpected(oauth2::error(text.error()));
            }
            if (!res->ok()) {
                co_return unexpected(oauth2::error(string("http_status"), string(std::string_view(std::to_string(res->status()))), string(), res->status()));
            }
            auto set = crypto::jose::jwk_set::parse(*text);
            if (!set) {
                co_return unexpected(detail::invalid("invalid_jwks", set.error().message()));
            }
            tracked_ptr box = make_tracked<detail::KeySet>();
            box->set = *set;
            s->keys.store(box);
            co_return expected<void, oauth2::error>();
        }

        // The keys fetched, under the lock, when the set is still the one the
        // caller saw and a minute has passed since the last try (or there has
        // been none); the set to verify with after it
        static async::task<expected<tracked_ptr<detail::KeySet>, oauth2::error>> _co_refresh_keys(tracked_ptr<detail::ProviderState> s,
                                                                                                  tracked_ptr<detail::KeySet> seen) noexcept {
            auto guard = co_await s->lock.scoped_lock();
            tracked_ptr<detail::KeySet> now = s->keys.load();
            const int64_t last = s->fetched_ms.load();
            if (now == seen && (last == 0 || time::now().unix_milli() - last >= 60000)) {
                auto f = co_await _co_fetch_keys(s);
                now = s->keys.load();
                if (!f && !now) {
                    co_return unexpected(f.error());
                }
            }
            if (!now) {
                co_return unexpected(detail::invalid("invalid_jwks", "the provider's keys could not be fetched"));
            }
            co_return now;
        }

        static async::task<expected<id_token, oauth2::error>> _co_verify(tracked_ptr<detail::ProviderState> s, string raw, verify_options o) noexcept {
            tracked_ptr<detail::KeySet> keys = s->keys.load();
            if (!keys) {
                auto k = co_await _co_refresh_keys(s, keys);
                if (!k) {
                    co_return unexpected(k.error());
                }
                keys = *k;
            }
            crypto::jose::jwt::verify_options vo;
            vo.issuer = s->issuer;
            vo.audience = o.client_id;
            vo.leeway = o.leeway;
            vo.require_expiration = true;
            auto t = crypto::jose::jwt::verify(raw, keys->set, vo);
            if (!t) {
                // a kid the set lacks: the provider rotated its keys, fetched again (at most once a minute)
                auto peek = crypto::jose::jwt::parse_unverified(raw);
                const string kid = peek ? peek->header()["kid"].as_string("") : string();
                if (!kid.empty() && !keys->set.find(kid)) {
                    auto k = co_await _co_refresh_keys(s, keys);
                    if (k && *k != keys) {
                        t = crypto::jose::jwt::verify(raw, (*k)->set, vo);
                    }
                }
            }
            if (!t) {
                co_return unexpected(detail::invalid("invalid_id_token", t.error().message()));
            }
            id_token out;
            out.claims = t->claims();
            out.issuer = t->issuer();
            out.subject = t->subject();
            out.audience = t->audience();
            out.nonce = out.claims["nonce"].as_string("");
            auto iat = t->issued_at();
            if (out.subject.empty() || !iat) {
                co_return unexpected(detail::invalid("invalid_id_token", "an ID token has sub and iat (Core §2)"));
            }
            out.issued_at = *iat;
            out.expiry = *t->expires_at();
            const string azp = out.claims["azp"].as_string("");
            if ((out.audience.size() > 1 || !azp.empty()) && azp != o.client_id) {
                co_return unexpected(detail::invalid("invalid_id_token", "azp is not the client"));
            }
            if (!o.nonce.empty() && out.nonce != o.nonce) {
                co_return unexpected(detail::invalid("invalid_id_token", "the nonce is not the request's"));
            }
            co_return out;
        }

        static async::task<expected<encoding::json, oauth2::error>> _co_userinfo(tracked_ptr<detail::ProviderState> s, oauth2::token t,
                                                                                 string subject) noexcept {
            if (s->userinfo.empty()) {
                co_return unexpected(oauth2::error(io::error(std::make_error_code(std::errc::invalid_argument), "oidc", "no userinfo endpoint")));
            }
            http::request req("GET", s->userinfo);
            req.set_header("Authorization", string::concat("Bearer ", t.access_token));
            auto v = co_await detail::get_json(s->http, req);
            if (!v) {
                co_return unexpected(v.error());
            }
            if (!subject.empty() && (*v)["sub"].as_string("") != subject) {
                co_return unexpected(detail::invalid("invalid_userinfo", "sub is not the ID token's (Core §5.3.2)"));
            }
            co_return std::move(*v);
        }

        tracked_ptr<detail::ProviderState> _s;
    };
}
