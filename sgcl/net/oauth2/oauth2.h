//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../http/client.h"
#include "../http/request.h"
#include "../http/response.h"
#include "../http/detail/auth.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/mutex.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/random.h"
#include "../../crypto/sha256.h"
#include "../../encoding/base64.h"
#include "../../encoding/json.h"
#include "../../time/datetime.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

// An OAuth 2.0 client (RFC 6749, 6750) of an authorization server: the
// authorization code grant with PKCE (RFC 7636), the client credentials
// grant, the device authorization grant (RFC 8628), refresh, revocation
// (RFC 7009) and introspection (RFC 7662); a token source that refreshes by
// itself, and an http::client that sends its tokens. Its own namespace,
// sgcl::net::oauth2, as net::acme's.
namespace sgcl::net::oauth2 {
    // An error of the protocol (RFC 6749 §5.2): the server's code
    // ("invalid_grant", "authorization_pending", ...), its description and
    // uri, the HTTP status; or a failure of the exchange itself (the
    // connection, a response that is not one of OAuth), then in transport()
    class error {
    public:
        error() noexcept = default;

        error(const string& code, const string& description, const string& uri = {}, int status = 0) noexcept
        : _code(code), _description(description), _uri(uri), _status(status) {
        }

        explicit error(const io::error& transport) noexcept
        : _transport(transport) {
        }

        SGCL_INLINE_HOT string code() const noexcept {
            return _code;
        }

        SGCL_INLINE_HOT string description() const noexcept {
            return _description;
        }

        SGCL_INLINE_HOT string uri() const noexcept {
            return _uri;
        }

        SGCL_INLINE_HOT int status() const noexcept {
            return _status;
        }

        SGCL_INLINE_HOT optional<io::error> transport() const noexcept {
            return _transport;
        }

        // "invalid_grant: the code expired", or the transport's message
        string message() const noexcept {
            if (_transport) {
                return _transport->message();
            }
            if (_description.empty()) {
                return _code;
            }
            return string::concat(_code, ": ", _description);
        }

    private:
        string _code;
        string _description;
        string _uri;
        int _status = 0;
        optional<io::error> _transport;
    };

    // A token of the token endpoint (RFC 6749 §5.1): the access token, its
    // type, a refresh token, the scope granted, OpenID Connect's ID token,
    // when it expires (now + expires_in when it was received), and the whole
    // response for the server's own fields
    struct token {
        string access_token;
        string token_type = "Bearer";
        string refresh_token;
        string scope;
        string id_token;
        optional<time::datetime> expiry;
        encoding::json raw;

        // An access token not expired, with 10 seconds of margin (a token
        // that dies on its way is no use): x/oauth2's rule
        bool valid() const noexcept {
            if (access_token.empty()) {
                return false;
            }
            return !expiry || time::now().unix_milli() + 10000 < expiry->unix_milli();
        }
    };

    // PKCE (RFC 7636): a verifier of 43 characters (256 random bits,
    // base64url) and its S256 challenge
    struct pkce {
        string verifier;
        string challenge;

        static pkce generate() {
            byte raw[32];
            crypto::random::fill(slice<byte>(raw, sizeof raw));
            pkce out;
            out.verifier = encoding::base64::raw_url.encode(slice<const byte>(raw, sizeof raw));
            auto h = crypto::sha256::of(out.verifier);
            out.challenge = encoding::base64::raw_url.encode(slice<const byte>(h.data(), h.size()));
            return out;
        }
    };

    // RFC 8628 §3.2: what the device shows its user, and what it polls with
    struct device_authorization {
        string device_code;
        string user_code;
        string verification_uri;
        string verification_uri_complete;
        time::datetime expiry;
        duration interval = std::chrono::seconds(5);
    };

    // RFC 7662 §2.2: what the server says of a token
    struct introspection {
        bool active = false;
        string scope;
        string client_id;
        string username;
        string token_type;
        string subject;
        string issuer;
        vector<string> audience;
        optional<time::datetime> expiry;
        optional<time::datetime> issued_at;
        optional<time::datetime> not_before;
        encoding::json raw;
    };

    // How the client proves itself to the token endpoint (RFC 6749 §2.3.1)
    enum class client_auth : uint8_t {
        basic,   // client_secret_basic: Authorization: Basic of the id and the secret (form-urlencoded first), the default
        post,    // client_secret_post: client_id and client_secret in the form
        none,    // a public client: client_id in the form alone
    };

    // The endpoints of an authorization server (RFC 8414's names)
    struct endpoints {
        string authorization;
        string token;
        string device_authorization;
        string revocation;
        string introspection;
    };

    class token_source;
    class config;

    namespace detail {
        using namespace sgcl::net::http::detail;

        // A value as application/x-www-form-urlencoded writes it
        inline string form_escape(const string& v) {
            net::query_params q;
            (void)q.add("v", v);
            const string all = q.to_string();
            return string(all.view().substr(2));
        }

        // A form urlencoded (application/x-www-form-urlencoded)
        inline string form_of(const vector<pair<string, string>>& fields) {
            net::query_params q;
            for (auto& f : fields) {
                (void)q.add(f.first, f.second);
            }
            return q.to_string();
        }

        inline optional<time::datetime> seconds_from_now(const encoding::json& v) noexcept {
            if (auto n = v.as_int(); n && *n > 0) {
                return time::datetime::from_unix_milli(time::now().unix_milli() + *n * 1000, time::zone::utc());
            }
            return nullopt;
        }

        inline optional<time::datetime> unix_time(const encoding::json& v) noexcept {
            if (auto n = v.as_int()) {
                return time::datetime::from_unix(*n, time::zone::utc());
            }
            return nullopt;
        }

        // A response of the server read: a 2xx's JSON object, or its error
        // (RFC 6749 §5.2: a JSON object with "error"; anything else an error
        // of the transport, net::errc::malformed_response or the status)
        inline async::task<expected<encoding::json, error>> read_json(http::response res, bool empty_ok) noexcept {
            auto body = co_await res.async_text();
            if (!body) {
                co_return unexpected(error(body.error()));
            }
            const int st = res.status();
            if (st >= 200 && st < 300 && empty_ok && body->empty()) {
                co_return encoding::json();
            }
            auto v = encoding::json::parse(*body);
            if (st >= 200 && st < 300) {
                if (!v || !v->is_object()) {
                    co_return unexpected(error(net::detail::net_error(net::errc::malformed_response, "POST", res.url().to_string())));
                }
                co_return std::move(*v);
            }
            if (v && v->is_object() && (*v)["error"].as_string()) {
                co_return unexpected(error((*v)["error"].as_string(""), (*v)["error_description"].as_string(""), (*v)["error_uri"].as_string(""), st));
            }
            co_return unexpected(error(string("http_status"), string(std::string_view(std::to_string(st))), string(), st));
        }

        // A token response read (RFC 6749 §5.1)
        inline expected<token, error> token_of(const encoding::json& v) {
            token t;
            t.access_token = v["access_token"].as_string("");
            if (t.access_token.empty()) {
                return unexpected(error(string("invalid_response"), string("no access_token")));
            }
            t.token_type = v["token_type"].as_string("Bearer");
            t.refresh_token = v["refresh_token"].as_string("");
            t.scope = v["scope"].as_string("");
            t.id_token = v["id_token"].as_string("");
            t.expiry = seconds_from_now(v["expires_in"]);
            t.raw = v;
            return t;
        }

        // A POST of a form to an endpoint with the client's authentication
        inline async::task<expected<encoding::json, error>> post_form(const config& c, string endpoint, vector<pair<string, string>> fields,
                                                                      bool empty_ok = false) noexcept;
    }

    // A client of an authorization server (x/oauth2's Config): a value, its
    // fields set by name
    class config {
    public:
        string client_id;
        string client_secret;
        oauth2::endpoints endpoints;
        string redirect_url;
        vector<string> scopes;
        oauth2::client_auth auth = client_auth::basic;
        http::client http;                            // the client of the server's endpoints: its TLS, proxy, timeouts

        // The authorization request's URL (RFC 6749 §4.1.1): response_type
        // code, the client, the redirect, the scopes, the state, the PKCE
        // challenge (S256), and the extra parameters (OpenID's nonce, a
        // prompt) after them
        string authorization_url(const string& state, const pkce& p, const vector<pair<string, string>>& extra = {}) const noexcept {
            net::query_params q;
            (void)q.add("response_type", "code");
            (void)q.add("client_id", client_id);
            if (!redirect_url.empty()) {
                (void)q.add("redirect_uri", redirect_url);
            }
            if (!scopes.empty()) {
                (void)q.add("scope", string::join(scopes, " "));
            }
            (void)q.add("state", state);
            if (!p.challenge.empty()) {
                (void)q.add("code_challenge", p.challenge);
                (void)q.add("code_challenge_method", "S256");
            }
            for (auto& e : extra) {
                (void)q.add(e.first, e.second);
            }
            const std::string_view sep = endpoints.authorization.view().find('?') == std::string_view::npos ? "?" : "&";
            return string::concat(endpoints.authorization, sep, q.to_string());
        }

        // The code of the redirect exchanged for a token (§4.1.3), with the
        // PKCE verifier
        expected<token, error> exchange(const string& code, const pkce& p) const {
            return async_exchange(code, p).wait();
        }

        async::task<expected<token, error>> async_exchange(string code, pkce p) const noexcept {
            return _co_token(*this, {{string("grant_type"), string("authorization_code")}, {string("code"), code},
                                     {string("redirect_uri"), redirect_url}, {string("code_verifier"), p.verifier}});
        }

        // The client's own token (§4.4)
        expected<token, error> client_credentials() const {
            return async_client_credentials().wait();
        }

        async::task<expected<token, error>> async_client_credentials() const noexcept {
            vector<pair<string, string>> f = {{string("grant_type"), string("client_credentials")}};
            if (!scopes.empty()) {
                f.push_back({string("scope"), string::join(scopes, " ")});
            }
            return _co_token(*this, std::move(f));
        }

        // The codes of the device flow (RFC 8628 §3.1)
        expected<device_authorization, error> device_authorize() const {
            return async_device_authorize().wait();
        }

        async::task<expected<device_authorization, error>> async_device_authorize() const noexcept {
            return _co_device(*this);
        }

        // The token of the device flow polled for (§3.4, §3.5): every
        // interval, five seconds more on slow_down, until the user approves
        // (the token), denies (access_denied), the codes expire
        // (expired_token), or the stop ends it (ECANCELED in transport())
        expected<token, error> device_token(const device_authorization& d, const async::stop_token& stop = {}) const {
            return async_device_token(d, stop).wait();
        }

        async::task<expected<token, error>> async_device_token(device_authorization d, async::stop_token stop = {}) const noexcept {
            return _co_device_token(*this, std::move(d), std::move(stop));
        }

        // A new access token of a refresh token (§6); the refresh token kept
        // when the server sends no new one
        expected<token, error> refresh(const token& t) const {
            return async_refresh(t).wait();
        }

        async::task<expected<token, error>> async_refresh(token t) const noexcept {
            return _co_refresh(*this, std::move(t));
        }

        // A token revoked (RFC 7009 §2.1); the hint is "access_token" or
        // "refresh_token"
        expected<void, error> revoke(const string& tok, const string& hint = {}) const {
            return async_revoke(tok, hint).wait();
        }

        async::task<expected<void, error>> async_revoke(string tok, string hint = {}) const noexcept {
            return _co_revoke(*this, std::move(tok), std::move(hint));
        }

        // What the server says of a token (RFC 7662)
        expected<introspection, error> introspect(const string& tok) const {
            return async_introspect(tok).wait();
        }

        async::task<expected<introspection, error>> async_introspect(string tok) const noexcept {
            return _co_introspect(*this, std::move(tok));
        }

        // A source of valid tokens from a token (refreshed by its refresh
        // token), or of the client's credentials (a new one when it expires)
        token_source source(const token& t) const;
        token_source source() const;

    private:
        static async::task<expected<token, error>> _co_token(config c, vector<pair<string, string>> fields) noexcept {
            auto v = co_await detail::post_form(c, c.endpoints.token, std::move(fields));
            if (!v) {
                co_return unexpected(v.error());
            }
            co_return detail::token_of(*v);
        }

        static async::task<expected<token, error>> _co_refresh(config c, token t) noexcept {
            if (t.refresh_token.empty()) {
                co_return unexpected(error(string("invalid_grant"), string("no refresh token")));
            }
            vector<pair<string, string>> f = {{string("grant_type"), string("refresh_token")}, {string("refresh_token"), t.refresh_token}};
            auto v = co_await detail::post_form(c, c.endpoints.token, std::move(f));
            if (!v) {
                co_return unexpected(v.error());
            }
            auto fresh = detail::token_of(*v);
            if (fresh && fresh->refresh_token.empty()) {
                fresh->refresh_token = t.refresh_token;
            }
            co_return fresh;
        }

        static async::task<expected<device_authorization, error>> _co_device(config c) noexcept {
            vector<pair<string, string>> f;
            if (!c.scopes.empty()) {
                f.push_back({string("scope"), string::join(c.scopes, " ")});
            }
            auto v = co_await detail::post_form(c, c.endpoints.device_authorization, std::move(f));
            if (!v) {
                co_return unexpected(v.error());
            }
            device_authorization d;
            d.device_code = (*v)["device_code"].as_string("");
            d.user_code = (*v)["user_code"].as_string("");
            d.verification_uri = (*v)["verification_uri"].as_string("");
            d.verification_uri_complete = (*v)["verification_uri_complete"].as_string("");
            if (d.device_code.empty() || d.user_code.empty() || d.verification_uri.empty()) {
                co_return unexpected(error(string("invalid_response"), string("no device_code, user_code or verification_uri")));
            }
            d.expiry = detail::seconds_from_now((*v)["expires_in"]).value_or(time::datetime::from_unix_milli(time::now().unix_milli() + 600000, time::zone::utc()));
            if (auto i = (*v)["interval"].as_int(); i && *i > 0) {
                d.interval = std::chrono::seconds(*i);
            }
            co_return d;
        }

        static async::task<expected<token, error>> _co_device_token(config c, device_authorization d, async::stop_token stop) noexcept {
            duration interval = d.interval;
            for (;;) {
                if (stop.stop_requested()) {
                    co_return unexpected(error(io::error(std::make_error_code(std::errc::operation_canceled), "device_token", c.endpoints.token)));
                }
                if (time::now().unix_milli() >= d.expiry.unix_milli()) {
                    co_return unexpected(error(string("expired_token"), string("the device code expired before the user approved")));
                }
                // the interval slept in slices, so that a stop ends the wait soon
                for (int64_t left = interval.nanoseconds(); left > 0 && !stop.stop_requested(); left -= 100000000) {
                    co_await async::sleep(duration(std::chrono::nanoseconds(left < 100000000 ? left : 100000000)));
                }
                if (stop.stop_requested()) {
                    continue;
                }
                vector<pair<string, string>> f = {{string("grant_type"), string("urn:ietf:params:oauth:grant-type:device_code")},
                                                  {string("device_code"), d.device_code}};
                auto v = co_await detail::post_form(c, c.endpoints.token, std::move(f));
                if (v) {
                    co_return detail::token_of(*v);
                }
                const string code = v.error().code();
                if (code == "authorization_pending") {
                    continue;
                }
                if (code == "slow_down") {
                    interval = interval + duration(std::chrono::seconds(5));   // RFC 8628 §3.5
                    continue;
                }
                co_return unexpected(v.error());
            }
        }

        static async::task<expected<void, error>> _co_revoke(config c, string tok, string hint) noexcept {
            vector<pair<string, string>> f = {{string("token"), tok}};
            if (!hint.empty()) {
                f.push_back({string("token_type_hint"), hint});
            }
            auto v = co_await detail::post_form(c, c.endpoints.revocation, std::move(f), true);
            if (!v) {
                co_return unexpected(v.error());
            }
            co_return expected<void, error>();
        }

        static async::task<expected<introspection, error>> _co_introspect(config c, string tok) noexcept {
            auto v = co_await detail::post_form(c, c.endpoints.introspection, {{string("token"), tok}});
            if (!v) {
                co_return unexpected(v.error());
            }
            const encoding::json& j = *v;
            introspection out;
            out.active = j["active"].as_bool(false);
            out.scope = j["scope"].as_string("");
            out.client_id = j["client_id"].as_string("");
            out.username = j["username"].as_string("");
            out.token_type = j["token_type"].as_string("");
            out.subject = j["sub"].as_string("");
            out.issuer = j["iss"].as_string("");
            if (j["aud"].is_array()) {
                for (size_t i = 0; i < j["aud"].size(); ++i) {
                    out.audience.push_back(j["aud"][i].as_string(""));
                }
            } else if (auto a = j["aud"].as_string()) {
                out.audience.push_back(*a);
            }
            out.expiry = detail::unix_time(j["exp"]);
            out.issued_at = detail::unix_time(j["iat"]);
            out.not_before = detail::unix_time(j["nbf"]);
            out.raw = j;
            co_return out;
        }
    };

    namespace detail {
        inline async::task<expected<encoding::json, error>> post_form(const config& c, string endpoint, vector<pair<string, string>> fields,
                                                                      bool empty_ok) noexcept {
            config cc = c;
            if (endpoint.empty()) {
                co_return unexpected(error(io::error(std::make_error_code(std::errc::invalid_argument), "oauth2", "no endpoint for the request")));
            }
            if (cc.auth != client_auth::basic) {
                fields.push_back({string("client_id"), cc.client_id});
                if (cc.auth == client_auth::post && !cc.client_secret.empty()) {
                    fields.push_back({string("client_secret"), cc.client_secret});
                }
            }
            http::request req("POST", endpoint);
            req.set_header("Content-Type", "application/x-www-form-urlencoded");
            req.set_header("Accept", "application/json");
            if (cc.auth == client_auth::basic) {
                // RFC 6749 §2.3.1: the id and the secret form-urlencoded before Basic
                req.set_basic_auth(form_escape(cc.client_id), form_escape(cc.client_secret));
            }
            req.set_body(form_of(fields));
            auto res = co_await cc.http.async_send(req);
            if (!res) {
                co_return unexpected(error(res.error()));
            }
            co_return co_await read_json(*res, empty_ok);
        }

        // What a token source keeps: the config, the token, a lock that lets
        // one refresh run at a time
        struct SourceState {
            config cfg;
            oauth2::token current;
            bool credentials = false;          // of client_credentials: a new token rather than a refresh
            async::mutex lock;
        };
    }

    // Valid tokens, refreshed when they expire, however many tasks ask at
    // once (one refresh at a time; the others wait for it and take its
    // token). A handle of one word: copies share the token
    class token_source {
    public:
        // The token now: the one held while it is valid, a refreshed one
        // when not (the refresh token, or a new client_credentials token)
        expected<oauth2::token, error> token() const {
            return async_token().wait();
        }

        async::task<expected<oauth2::token, error>> async_token() const noexcept {
            return _co_token(_s, false);
        }

        // The token refreshed now, whatever its expiry (a resource server
        // that answered invalid_token)
        async::task<expected<oauth2::token, error>> async_refresh() const noexcept {
            return _co_token(_s, true);
        }

        // An http::client that sends Authorization: Bearer of the source's
        // token with every request (refreshed when it expires) and answers a
        // 401 with error="invalid_token" by refreshing it and sending the
        // request again, once (x/oauth2's Config.Client). A copy of the
        // config's client, or of the one given (its settings, its pool)
        http::client client() const {
            return client(_s->cfg.http);
        }

        http::client client(const http::client& base) const;

    private:
        friend class config;

        explicit token_source(tracked_ptr<detail::SourceState> s) noexcept
        : _s(std::move(s)) {
        }

        static async::task<expected<oauth2::token, error>> _co_token(tracked_ptr<detail::SourceState> s, bool force) noexcept {
            auto guard = co_await s->lock.scoped_lock();
            if (!force && s->current.valid()) {
                co_return s->current;
            }
            expected<oauth2::token, error> fresh = s->credentials ? co_await s->cfg.async_client_credentials() : co_await s->cfg.async_refresh(s->current);
            if (!fresh) {
                co_return unexpected(fresh.error());
            }
            s->current = *fresh;
            co_return *fresh;
        }

        tracked_ptr<detail::SourceState> _s;
    };

    inline token_source config::source(const token& t) const {
        tracked_ptr s = make_tracked<detail::SourceState>();
        s->cfg = *this;
        s->current = t;
        return token_source(s);
    }

    inline token_source config::source() const {
        tracked_ptr s = make_tracked<detail::SourceState>();
        s->cfg = *this;
        s->credentials = true;
        return token_source(s);
    }

    inline http::client token_source::client(const http::client& base) const {
        http::client c = base;
        auto s = _s;
        http::detail::ClientAccess::set_authorization(c, [s](bool refresh) -> async::task<expected<string, io::error>> {
            auto t = co_await _co_token(s, refresh);
            if (!t) {
                co_return unexpected(t.error().transport() ? *t.error().transport()
                                                           : io::error(std::make_error_code(std::errc::permission_denied), "oauth2", t.error().message()));
            }
            co_return string::concat(t->token_type.empty() || detail::iequal(t->token_type.view(), "bearer") ? std::string_view("Bearer") : t->token_type.view(),
                                     " ", t->access_token);
        });
        return c;
    }
}
