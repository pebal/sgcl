//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "key.h"
#include "types.h"
#include "detail/parse.h"
#include "../http/client.h"
#include "../ip.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/timer.h"
#include "../../core/clock.h"
#include "../../core/make_tracked.h"
#include "../../txt/idna.h"

#include <charconv>
#include <mutex>
#include <string>
#include <string_view>

// The protocol of RFC 8555 from the client's side, Go's
// golang.org/x/crypto/acme: the directory, the replay nonces (a badNonce
// answered by a retry with the nonce of the error), every request a JWS of
// the account's key (kid once the account is known, the JWK before it);
// accounts (new, existing, updated, deactivated, their key rolled over,
// External Account Binding); orders of DNS names, wildcards and IP addresses
// (RFC 8738); authorizations and their challenges (http-01, dns-01,
// tls-alpn-01 of RFC 8737, with the values and the certificate each needs);
// finalization with a CSR; polling with the CA's Retry-After; the chain and
// its alternates; revocation by the account or by the certificate's key; the
// renewal information of RFC 9773. A problem document is an io::error of the
// acme category, its code the problem's type, its detail (and its
// subproblems') the error's text; a rate limit or a 503 whose Retry-After is
// within options::max_retry_after is waited for and the request sent again.
namespace sgcl::net::acme {
    class client;

    namespace detail {
        // The settings of a client and what it learns: the directory (asked
        // for once), the account's URL, the nonces the responses gave and no
        // request used yet; the key, which a rollover replaces
        struct ClientState {
            string directory_url;
            http::client http;
            string user_agent;
            int bad_nonce_retries = 5;
            duration max_retry_after;
            duration poll_interval;
            duration poll_timeout;

            std::mutex lock;
            account_key key;
            string kid;
            optional<acme::directory> dir;
            vector<string> nonces;

            ClientState(const string& url, const account_key& k) noexcept
            : directory_url(url), key(k) {
            }

            account_key current_key() noexcept {
                std::lock_guard<std::mutex> g(lock);
                return key;
            }

            string current_kid() noexcept {
                std::lock_guard<std::mutex> g(lock);
                return kid;
            }

            void set_kid(const string& k) noexcept {
                std::lock_guard<std::mutex> g(lock);
                kid = k;
            }

            void keep_nonce(const string& n) noexcept {
                if (n.empty()) {
                    return;
                }
                std::lock_guard<std::mutex> g(lock);
                if (nonces.size() >= 16) {
                    nonces.erase(nonces.begin());
                }
                nonces.push_back(n);
            }

            optional<string> take_nonce() noexcept {
                std::lock_guard<std::mutex> g(lock);
                if (nonces.empty()) {
                    return nullopt;
                }
                string n = nonces.back();
                nonces.pop_back();
                return n;
            }
        };

        // What a request got back: the status, the fields, the body, and the
        // links read from them
        struct Reply {
            int status = 0;
            http::headers headers;
            string body;
            string location;                 // Location, resolved against the request's URL
            string link_up;                  // Link rel="up"
            vector<string> alternates;       // Link rel="alternate"
            duration retry_after = duration::zero();
        };

        // Retry-After (RFC 9110 §10.2.3): seconds or an HTTP date; zero for
        // none or one that does not read
        inline duration retry_after_of(const http::headers& h) noexcept {
            string v = h.get(string("Retry-After"));
            if (v.empty()) {
                return duration::zero();
            }
            std::string_view s = v.view();
            uint64_t n = 0;
            auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
            if (ec == std::errc() && p == s.data() + s.size()) {
                return n > 86400ull * 365 ? duration(365 * 24 * hour) : duration(int64_t(n) * second);
            }
            if (auto t = h.date(string("Retry-After"))) {
                int64_t d = t->unix() - time::now().unix();
                return d <= 0 ? duration::zero() : d > 86400ll * 365 ? duration(365 * 24 * hour) : duration(d * second);
            }
            return duration::zero();
        }

        // A URL of a field resolved against the URL of the request
        inline string resolved(const string& base, const string& ref) noexcept {
            if (ref.empty()) {
                return ref;
            }
            auto b = net::url::parse(base);
            if (!b) {
                return ref;
            }
            auto r = b->resolve(ref);
            return r ? r->to_string() : ref;
        }

        // The links of a response's Link fields (RFC 8288) of a relation:
        // <uri-reference>; rel="name" (or rel=name), several per field
        inline vector<string> links_of(const http::headers& h, std::string_view rel, const string& base) noexcept {
            vector<string> out;
            for (const string& field : h.get_all(string("Link"))) {
                std::string_view v = field.view();
                size_t at = 0;
                while (at < v.size()) {
                    size_t lt = v.find('<', at);
                    if (lt == std::string_view::npos) {
                        break;
                    }
                    size_t gt = v.find('>', lt);
                    if (gt == std::string_view::npos) {
                        break;
                    }
                    std::string_view target = v.substr(lt + 1, gt - lt - 1);
                    size_t next = v.find('<', gt);
                    std::string_view params = v.substr(gt + 1, next == std::string_view::npos ? std::string_view::npos : next - gt - 1);
                    // the parameters up to the next link: rel among them
                    size_t r = 0;
                    bool match = false;
                    while ((r = params.find("rel", r)) != std::string_view::npos) {
                        size_t eq = params.find_first_not_of(" \t", r + 3);
                        if (eq == std::string_view::npos || params[eq] != '=') {
                            r += 3;
                            continue;
                        }
                        size_t val = params.find_first_not_of(" \t", eq + 1);
                        if (val == std::string_view::npos) {
                            break;
                        }
                        std::string_view name;
                        if (params[val] == '"') {
                            size_t close = params.find('"', val + 1);
                            name = params.substr(val + 1, close == std::string_view::npos ? std::string_view::npos : close - val - 1);
                        } else {
                            size_t end = params.find_first_of(" \t;,", val);
                            name = params.substr(val, end == std::string_view::npos ? std::string_view::npos : end - val);
                        }
                        // a rel may list several relations, space-separated
                        size_t w = 0;
                        while (w < name.size()) {
                            size_t sp = name.find(' ', w);
                            std::string_view one = name.substr(w, sp == std::string_view::npos ? std::string_view::npos : sp - w);
                            if (one.size() == rel.size() && http::detail::iequal(one, rel)) {
                                match = true;
                            }
                            if (sp == std::string_view::npos) {
                                break;
                            }
                            w = sp + 1;
                        }
                        break;
                    }
                    if (match) {
                        out.push_back(resolved(base, string(target)));
                    }
                    if (next == std::string_view::npos) {
                        break;
                    }
                    at = next;
                }
            }
            return out;
        }

        // The error of a problem document: its code, the operation, the
        // detail (and each subproblem's, with its identifier); a Retry-After
        // of it said in the text
        inline io::error problem_error(const problem& p, const char* op, duration retry_after) noexcept {
            std::string what(p.detail.view());
            for (auto& s : p.subproblems) {
                what += what.empty() ? "" : "; ";
                if (!s.identifier.value.empty()) {
                    what += std::string(s.identifier.value.view()) + ": ";
                }
                what += std::string(s.detail.view());
            }
            if (retry_after > duration::zero()) {
                what += (what.empty() ? "" : " ") + std::string("(retry after ") + std::to_string(whole_seconds(retry_after)) + " s)";
            }
            return acme_error(p.code() == errc::unknown_problem ? errc::unknown_problem : p.code(), op, string(what));
        }

        inline bool is_problem_response(const Reply& r) noexcept {
            return r.status >= 400;
        }

        // A plain GET (the directory, a renewalInfo, which RFC 8555 and RFC
        // 9773 leave unauthenticated), a problem document its error
        inline async::task<expected<Reply, io::error>> co_get(tracked_ptr<ClientState> s, string url, const char* op) noexcept {
            http::request req(string("GET"), url);
            req.set_header(string("User-Agent"), s->user_agent);
            for (int attempt = 0;; ++attempt) {
                auto res = co_await s->http.async_send(req);
                if (!res) {
                    co_return unexpected(res.error());
                }
                Reply r;
                r.status = res->status();
                r.headers = res->headers();
                r.retry_after = retry_after_of(r.headers);
                s->keep_nonce(r.headers.get(string("Replay-Nonce")));
                auto body = co_await res->async_text();
                if (!body) {
                    co_return unexpected(body.error());
                }
                r.body = *body;
                if (r.status >= 400) {
                    problem p;
                    if (auto j = json::parse(r.body)) {
                        if (auto pp = parse_problem(*j)) {
                            p = *pp;
                        }
                    }
                    if (p.type.empty()) {
                        p.type = string("about:blank");
                        p.detail = string::concat("HTTP ", string(std::to_string(r.status)));
                    }
                    if ((p.code() == errc::rate_limited || r.status == 503) && r.retry_after > duration::zero() && r.retry_after <= s->max_retry_after && attempt < 3) {
                        co_await async::sleep(r.retry_after);
                        continue;
                    }
                    co_return unexpected(problem_error(p, op, r.retry_after));
                }
                co_return r;
            }
        }

        inline async::task<expected<acme::directory, io::error>> co_directory(tracked_ptr<ClientState> s) noexcept {
            {
                std::lock_guard<std::mutex> g(s->lock);
                if (s->dir) {
                    co_return *s->dir;
                }
            }
            auto r = co_await co_get(s, s->directory_url, "acme directory");
            if (!r) {
                co_return unexpected(r.error());
            }
            auto j = parse_json(r->body);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto d = parse_directory(*j);
            if (!d) {
                co_return unexpected(d.error());
            }
            d->new_nonce = resolved(s->directory_url, d->new_nonce);
            d->new_account = resolved(s->directory_url, d->new_account);
            d->new_order = resolved(s->directory_url, d->new_order);
            d->new_authz = resolved(s->directory_url, d->new_authz);
            d->revoke_cert = resolved(s->directory_url, d->revoke_cert);
            d->key_change = resolved(s->directory_url, d->key_change);
            d->renewal_info = resolved(s->directory_url, d->renewal_info);
            std::lock_guard<std::mutex> g(s->lock);
            s->dir = *d;
            co_return *d;
        }

        // A fresh nonce (§7.2): one a response gave, else a HEAD of newNonce
        inline async::task<expected<string, io::error>> co_nonce(tracked_ptr<ClientState> s) noexcept {
            if (auto n = s->take_nonce()) {
                co_return *n;
            }
            auto d = co_await co_directory(s);
            if (!d) {
                co_return unexpected(d.error());
            }
            http::request req(string("HEAD"), d->new_nonce);
            req.set_header(string("User-Agent"), s->user_agent);
            auto res = co_await s->http.async_send(req);
            if (!res) {
                co_return unexpected(res.error());
            }
            string n = res->header(string("Replay-Nonce"));
            if (n.empty()) {
                co_return unexpected(acme_error(errc::malformed_response, "acme new-nonce", string("no Replay-Nonce in the answer of newNonce")));
            }
            co_return n;
        }

        // How a request is signed: by the account (kid), by the key alone
        // (jwk: newAccount, a revocation by a certificate's key)
        enum class Signer : uint8_t { account, jwk };

        // A signed POST (§6.2) of a payload ("" for a POST-as-GET, §6.3):
        // the nonce, the JWS, the answer; a badNonce sent again with the
        // nonce of its answer, up to bad_nonce_retries times; a rate limit or
        // a 503 with a Retry-After within max_retry_after waited for and sent
        // again, three times at most. `key_override` signs in place of the
        // account's key (a certificate's key, by jwk)
        inline async::task<expected<Reply, io::error>> co_post(tracked_ptr<ClientState> s, string url, string payload, Signer signer, const char* op,
                                                               tracked_ptr<const KeyState> key_override = {}, string accept = {}) noexcept {
            int bad_nonces = 0;
            int waits = 0;
            for (;;) {
                auto nonce = co_await co_nonce(s);
                if (!nonce) {
                    co_return unexpected(nonce.error());
                }
                account_key key = s->current_key();
                const KeyState& ks = key_override ? *key_override : KeyAccess::state(key);
                string kid = signer == Signer::account ? s->current_kid() : string();
                if (signer == Signer::account && kid.empty()) {
                    co_return unexpected(acme_error(errc::account_does_not_exist, op, string("no account: register_account first, or options::account_url")));
                }
                string body = jws(*ks.key, header(ks, kid, *nonce, url), payload);
                http::request req(string("POST"), url);
                req.set_header(string("Content-Type"), string("application/jose+json"));
                req.set_header(string("User-Agent"), s->user_agent);
                if (!accept.empty()) {
                    req.set_header(string("Accept"), accept);
                }
                req.set_body(body);
                auto res = co_await s->http.async_send(req);
                if (!res) {
                    co_return unexpected(res.error());
                }
                Reply r;
                r.status = res->status();
                r.headers = res->headers();
                r.retry_after = retry_after_of(r.headers);
                s->keep_nonce(r.headers.get(string("Replay-Nonce")));
                auto text = co_await res->async_text();
                if (!text) {
                    co_return unexpected(text.error());
                }
                r.body = *text;
                if (r.status >= 400) {
                    problem p;
                    if (auto j = json::parse(r.body)) {
                        if (auto pp = parse_problem(*j)) {
                            p = *pp;
                        }
                    }
                    if (p.type.empty()) {
                        p.type = string("about:blank");
                        p.detail = string::concat("HTTP ", string(std::to_string(r.status)));
                    }
                    if (p.code() == errc::bad_nonce && bad_nonces < s->bad_nonce_retries) {
                        ++bad_nonces;
                        continue;   // the answer's nonce is in the pool now
                    }
                    if ((p.code() == errc::rate_limited || r.status == 503) && r.retry_after > duration::zero() && r.retry_after <= s->max_retry_after && waits < 3) {
                        ++waits;
                        co_await async::sleep(r.retry_after);
                        continue;
                    }
                    co_return unexpected(problem_error(p, op, r.retry_after));
                }
                r.location = resolved(url, r.headers.get(string("Location")));
                auto up = links_of(r.headers, "up", url);
                if (!up.empty()) {
                    r.link_up = up[0];
                }
                r.alternates = links_of(r.headers, "alternate", url);
                co_return r;
            }
        }

        // The JSON of a reply's body, the error a malformed_response of op
        inline expected<json, io::error> body_json(const Reply& r, const char* op) noexcept {
            auto j = json::parse(r.body);
            if (!j) {
                return unexpected(acme_error(errc::malformed_response, op, string::concat("a body that is not JSON: ", j.error().message())));
            }
            return std::move(*j);
        }

        template<class T>
        expected<T, io::error> with_op(expected<T, io::error> r, const char* op) noexcept {
            if (!r && r.error().code() == errc::malformed_response) {
                return unexpected(acme_error(errc::malformed_response, op, r.error().path()));
            }
            return r;
        }

        inline json strings_json(const vector<string>& v) noexcept {
            auto b = json::array({});
            for (auto& x : v) {
                b = b.push_back(json(x));
            }
            return b;
        }

        // newAccount (§7.3): the account of the key, made when there is none
        // (unless only_return_existing); its URL becomes the kid
        inline async::task<expected<account, io::error>> co_register(tracked_ptr<ClientState> s, account_options o) noexcept {
            const char* op = "acme new-account";
            auto d = co_await co_directory(s);
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<json::member> members;
            json payload = json::object({});
            if (!o.contact.empty()) {
                payload = payload.set(string("contact"), strings_json(o.contact));
            }
            if (o.terms_agreed) {
                payload = payload.set(string("termsOfServiceAgreed"), json(true));
            }
            if (o.only_return_existing) {
                payload = payload.set(string("onlyReturnExisting"), json(true));
            }
            if (o.external_account) {
                auto mac = encoding::base64::raw_url.decode(o.external_account->hmac_key);
                if (!mac) {
                    // a padded key, as some CAs print it
                    mac = encoding::base64::url.decode(o.external_account->hmac_key);
                }
                if (!mac || mac->empty() || o.external_account->key_id.empty()) {
                    co_return unexpected(acme_error(errc::malformed, op, string("an external account binding without a key id, or whose HMAC key is not base64url")));
                }
                account_key key = s->current_key();
                string eab_jws = eab(KeyAccess::state(key), o.external_account->key_id, mac->as_slice(), d->new_account);
                payload = payload.set(string("externalAccountBinding"), *json::parse(eab_jws));
            } else if (d->external_account_required && !o.only_return_existing) {
                co_return unexpected(acme_error(errc::external_account_required, op, string("the CA requires an external account binding (account_options::external_account)")));
            }
            auto r = co_await co_post(s, d->new_account, payload.to_string(), Signer::jwk, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (r->location.empty()) {
                co_return unexpected(acme_error(errc::malformed_response, op, string("no Location of the account")));
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto a = with_op(parse_account(*j, r->location), op);
            if (!a) {
                co_return unexpected(a.error());
            }
            s->set_kid(r->location);
            co_return *a;
        }

        // The kid, looked up (newAccount, onlyReturnExisting) when the client
        // does not know it yet
        inline async::task<expected<string, io::error>> co_kid(tracked_ptr<ClientState> s) noexcept {
            string kid = s->current_kid();
            if (!kid.empty()) {
                co_return kid;
            }
            account_options o;
            o.only_return_existing = true;
            auto a = co_await co_register(s, o);
            if (!a) {
                co_return unexpected(a.error());
            }
            co_return a->url;
        }

        inline async::task<expected<account, io::error>> co_account_post(tracked_ptr<ClientState> s, string payload, const char* op) noexcept {
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            auto r = co_await co_post(s, *kid, payload, Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            co_return with_op(parse_account(*j, *kid), op);
        }

        // keyChange (§7.3.5): the inner JWS of the new key (its JWK, the URL,
        // the account and the old key's JWK), inside the outer of the old
        inline async::task<expected<void, io::error>> co_change_key(tracked_ptr<ClientState> s, account_key next) noexcept {
            const char* op = "acme key-change";
            auto d = co_await co_directory(s);
            if (!d) {
                co_return unexpected(d.error());
            }
            if (d->key_change.empty()) {
                co_return unexpected(acme_error(errc::unsupported, op, string("the directory has no keyChange")));
            }
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            account_key old = s->current_key();
            const KeyState& nk = KeyAccess::state(next);
            string inner_header = json::object({{"alg", nk.alg}, {"jwk", nk.jwk_value}, {"url", d->key_change}}).to_string();
            string inner_payload = json::object({{"account", *kid}, {"oldKey", KeyAccess::state(old).jwk_value}}).to_string();
            string inner = jws(*nk.key, inner_header, inner_payload);
            auto r = co_await co_post(s, d->key_change, inner, Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            std::lock_guard<std::mutex> g(s->lock);
            s->key = next;
            co_return expected<void, io::error>();
        }

        // A name as an identifier: an IP address (RFC 8738, its text as
        // RFC 5952 writes it), else a DNS name in A-labels, lower case, a
        // trailing dot dropped, a wildcard's "*." kept
        inline expected<identifier, io::error> identifier_of_name(const string& name, const char* op) noexcept {
            std::string_view v = name.view();
            if (!v.empty() && v.back() == '.') {
                v.remove_suffix(1);
            }
            if (v.empty()) {
                return unexpected(acme_error(errc::rejected_identifier, op, string("an empty name")));
            }
            std::string_view host = v;
            if (host.size() > 1 && host.front() == '[' && host.back() == ']') {
                host = host.substr(1, host.size() - 2);
            }
            if (auto a = net::ip_address::parse(string(host)); a && a->zone().empty()) {
                return identifier{string("ip"), a->to_string()};
            }
            bool wildcard = v.size() > 2 && v.substr(0, 2) == "*.";
            std::string_view rest = wildcard ? v.substr(2) : v;
            bool ascii = true;
            for (unsigned char c : rest) {
                ascii &= c < 0x80;
            }
            std::string out;
            if (ascii) {
                out.assign(rest);
                for (char& c : out) {
                    if (c >= 'A' && c <= 'Z') {
                        c = char(c - 'A' + 'a');
                    }
                }
            } else {
                auto a = txt::idna::to_ascii(string(rest), txt::idna::options::whatwg());
                if (!a) {
                    return unexpected(acme_error(errc::rejected_identifier, op, string::concat("a name IDNA refuses: ", name)));
                }
                out.assign(a->view());
            }
            for (char c : out) {
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.')) {
                    return unexpected(acme_error(errc::rejected_identifier, op, string::concat("a name with a character a DNS name has not: ", name)));
                }
            }
            // labels of 1 to 63 characters, none starting or ending with a hyphen (RFC 1123 §2.1)
            bool labels_ok = !out.empty() && out.size() <= 253;
            for (size_t at = 0; labels_ok && at <= out.size();) {
                size_t dot = out.find('.', at);
                size_t end = dot == std::string::npos ? out.size() : dot;
                labels_ok = end > at && end - at <= 63 && out[at] != '-' && out[end - 1] != '-';
                if (dot == std::string::npos) {
                    break;
                }
                at = dot + 1;
            }
            if (!labels_ok) {
                return unexpected(acme_error(errc::rejected_identifier, op, string::concat("not a DNS name: ", name)));
            }
            return identifier{string("dns"), string(wildcard ? "*." + out : out)};
        }

        inline json identifier_json(const identifier& i) noexcept {
            return json::object({{"type", i.type}, {"value", i.value}});
        }

        inline async::task<expected<order, io::error>> co_new_order(tracked_ptr<ClientState> s, vector<string> names, order_options o) noexcept {
            const char* op = "acme new-order";
            if (names.empty()) {
                co_return unexpected(acme_error(errc::malformed, op, string("an order of no names")));
            }
            auto d = co_await co_directory(s);
            if (!d) {
                co_return unexpected(d.error());
            }
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            auto ids = json::array({});
            for (auto& n : names) {
                auto id = identifier_of_name(n, op);
                if (!id) {
                    co_return unexpected(id.error());
                }
                ids = ids.push_back(identifier_json(*id));
            }
            json payload = json::object({{"identifiers", ids}});
            if (o.not_before) {
                payload = payload.set(string("notBefore"), json(o.not_before->format(time::rfc3339)));
            }
            if (o.not_after) {
                payload = payload.set(string("notAfter"), json(o.not_after->format(time::rfc3339)));
            }
            if (!o.replaces.empty()) {
                if (d->renewal_info.empty()) {
                    co_return unexpected(acme_error(errc::unsupported, op, string("replaces: the directory has no renewalInfo (RFC 9773)")));
                }
                payload = payload.set(string("replaces"), json(o.replaces));
            }
            if (!o.profile.empty()) {
                payload = payload.set(string("profile"), json(o.profile));
            }
            auto r = co_await co_post(s, d->new_order, payload.to_string(), Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (r->location.empty()) {
                co_return unexpected(acme_error(errc::malformed_response, op, string("no Location of the order")));
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto out = with_op(parse_order(*j, r->location), op);
            if (out) {
                out->retry_after = r->retry_after;
                for (auto& a : out->authorizations) {
                    a = resolved(r->location, a);
                }
                out->finalize = resolved(r->location, out->finalize);
                out->certificate = resolved(r->location, out->certificate);
            }
            co_return out;
        }

        inline async::task<expected<order, io::error>> co_get_order(tracked_ptr<ClientState> s, string url) noexcept {
            const char* op = "acme order";
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            auto r = co_await co_post(s, url, string(), Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto out = with_op(parse_order(*j, url), op);
            if (out) {
                out->retry_after = r->retry_after;
                for (auto& a : out->authorizations) {
                    a = resolved(url, a);
                }
                out->finalize = resolved(url, out->finalize);
                out->certificate = resolved(url, out->certificate);
            }
            co_return out;
        }

        inline async::task<expected<authorization, io::error>> co_get_authorization(tracked_ptr<ClientState> s, string url, duration* retry_after = nullptr) noexcept {
            const char* op = "acme authorization";
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            auto r = co_await co_post(s, url, string(), Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (retry_after) {
                *retry_after = r->retry_after;
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto out = with_op(parse_authorization(*j, url), op);
            if (out) {
                for (auto& c : out->challenges) {
                    c.url = resolved(url, c.url);
                }
            }
            co_return out;
        }

        inline async::task<expected<challenge, io::error>> co_challenge_post(tracked_ptr<ClientState> s, string url, string payload, const char* op) noexcept {
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            auto r = co_await co_post(s, url, payload, Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto out = with_op(parse_challenge(*j), op);
            if (out) {
                out->url = resolved(url, out->url);
            }
            co_return out;
        }

        // The pause before the next poll: the CA's Retry-After, else the
        // client's interval, never past the deadline
        SGCL_INLINE_HOT duration poll_pause(duration retry_after, duration interval) noexcept {
            duration d = retry_after > duration::zero() ? retry_after : interval;
            return d < 50 * millisecond ? duration(50 * millisecond) : d;
        }

        inline io::error poll_timed_out(const char* op, const string& url) noexcept {
            return io::error(std::make_error_code(std::errc::timed_out), op, url);
        }

        // An authorization polled until it is no longer pending (§7.5.1):
        // valid, or errc::authorization_invalid with the failed challenge's
        // problem, or the status it ended in (deactivated, expired, revoked)
        inline async::task<expected<authorization, io::error>> co_wait_authorization(tracked_ptr<ClientState> s, string url) noexcept {
            const char* op = "acme authorization";
            const time_point deadline = sgcl::clock::now() + s->poll_timeout;
            for (;;) {
                duration ra = duration::zero();
                auto a = co_await co_get_authorization(s, url, &ra);
                if (!a) {
                    co_return unexpected(a.error());
                }
                if (a->status == status::valid) {
                    co_return *a;
                }
                if (a->status != status::pending) {
                    std::string what = std::string(a->identifier.value.view()) + ": the authorization is " + to_string(a->status);
                    for (auto& c : a->challenges) {
                        if (c.error) {
                            what += std::string("; ") + std::string(c.type.view()) + ": " + std::string(c.error->detail.view());
                        }
                    }
                    co_return unexpected(acme_error(errc::authorization_invalid, op, string(what)));
                }
                duration pause = poll_pause(ra, s->poll_interval);
                if (sgcl::clock::now() + pause > deadline) {
                    co_return unexpected(poll_timed_out(op, url));
                }
                co_await async::sleep(pause);
            }
        }

        // An order polled until it is no longer pending or processing
        // (§7.4): ready or valid; invalid is errc::order_invalid with the
        // order's problem
        inline async::task<expected<order, io::error>> co_wait_order(tracked_ptr<ClientState> s, string url) noexcept {
            const char* op = "acme order";
            const time_point deadline = sgcl::clock::now() + s->poll_timeout;
            for (;;) {
                auto o = co_await co_get_order(s, url);
                if (!o) {
                    co_return unexpected(o.error());
                }
                if (o->status == status::ready || o->status == status::valid) {
                    co_return *o;
                }
                if (o->status == status::invalid) {
                    std::string what = "the order is invalid";
                    if (o->error) {
                        what += ": " + std::string(problem_error(*o->error, op, duration::zero()).path().view());
                    }
                    co_return unexpected(acme_error(errc::order_invalid, op, string(what)));
                }
                duration pause = poll_pause(o->retry_after, s->poll_interval);
                if (sgcl::clock::now() + pause > deadline) {
                    co_return unexpected(poll_timed_out(op, url));
                }
                co_await async::sleep(pause);
            }
        }

        // finalize (§7.4): the CSR sent, the order polled until valid
        inline async::task<expected<order, io::error>> co_finalize(tracked_ptr<ClientState> s, order o, crypto::x509::certificate_request csr) noexcept {
            const char* op = "acme finalize";
            if (o.finalize.empty()) {
                co_return unexpected(acme_error(errc::malformed, op, string("an order without its finalize URL")));
            }
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            string payload = json::object({{"csr", b64(csr.raw())}}).to_string();
            auto r = co_await co_post(s, o.finalize, payload, Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto now = with_op(parse_order(*j, o.url), op);
            if (!now) {
                co_return unexpected(now.error());
            }
            if (now->status == status::valid) {
                now->certificate = resolved(o.url, now->certificate);
                co_return *now;
            }
            if (now->status == status::invalid) {
                std::string what = "the order is invalid";
                if (now->error) {
                    what += ": " + std::string(problem_error(*now->error, op, duration::zero()).path().view());
                }
                co_return unexpected(acme_error(errc::order_invalid, op, string(what)));
            }
            if (r->retry_after > duration::zero()) {
                co_await async::sleep(poll_pause(r->retry_after, s->poll_interval));
            }
            auto done = co_await co_wait_order(s, o.url);
            if (done && done->status != status::valid) {
                // ready again after a finalize: the CA did not take the CSR
                co_return unexpected(acme_error(errc::order_invalid, op, string("the order is ready again after its finalize")));
            }
            co_return done;
        }

        // The chain of a certificate URL (§7.4.2), its alternates by Link
        inline async::task<expected<certificate_chain, io::error>> co_certificate(tracked_ptr<ClientState> s, string url) noexcept {
            const char* op = "acme certificate";
            auto kid = co_await co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            auto r = co_await co_post(s, url, string(), Signer::account, op, {}, string("application/pem-certificate-chain"));
            if (!r) {
                co_return unexpected(r.error());
            }
            certificate_chain c;
            c.pem = r->body;
            c.alternates = r->alternates;
            auto blocks = encoding::pem::parse_all(c.pem);
            if (!blocks) {
                co_return unexpected(acme_error(errc::malformed_response, op, string::concat("a certificate chain that is not PEM: ", blocks.error().message())));
            }
            for (auto& b : *blocks) {
                if (b.type() != "CERTIFICATE") {
                    co_return unexpected(acme_error(errc::malformed_response, op, string("a block of the chain that is not a CERTIFICATE")));
                }
                auto cert = crypto::x509::certificate::parse(b.bytes().as_slice());
                if (!cert) {
                    co_return unexpected(acme_error(errc::malformed_response, op, string::concat("a certificate of the chain does not parse: ", cert.error().message())));
                }
                c.certificates.push_back(std::move(*cert));
            }
            if (c.certificates.empty()) {
                co_return unexpected(acme_error(errc::malformed_response, op, string("an empty certificate chain")));
            }
            co_return c;
        }

        // revokeCert (§7.6): by the account, or by the certificate's key
        // (its JWK in the header)
        inline async::task<expected<void, io::error>> co_revoke(tracked_ptr<ClientState> s, crypto::x509::certificate cert, revocation_reason reason,
                                                                tracked_ptr<const KeyState> certificate_key) noexcept {
            const char* op = "acme revoke-cert";
            auto d = co_await co_directory(s);
            if (!d) {
                co_return unexpected(d.error());
            }
            if (d->revoke_cert.empty()) {
                co_return unexpected(acme_error(errc::unsupported, op, string("the directory has no revokeCert")));
            }
            json payload = json::object({{"certificate", b64(cert.raw())}});
            if (reason != revocation_reason::unspecified) {
                payload = payload.set(string("reason"), json(int64_t(reason)));
            }
            if (!certificate_key) {
                auto kid = co_await co_kid(s);
                if (!kid) {
                    co_return unexpected(kid.error());
                }
            }
            auto r = co_await co_post(s, d->revoke_cert, payload.to_string(), certificate_key ? Signer::jwk : Signer::account, op, certificate_key);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();
        }

        // The ARI identifier of a certificate (RFC 9773 §4.1): the key
        // identifier of its authorityKeyIdentifier and its serial number's
        // DER content, each base64url, joined by a dot
        inline expected<string, io::error> renewal_id_of(const crypto::x509::certificate& cert) noexcept {
            const auto& aki = cert.authority_key_id();
            if (aki.empty()) {
                return unexpected(acme_error(errc::malformed, "acme renewal-info", string("a certificate without an authorityKeyIdentifier")));
            }
            return string::concat(b64(aki.as_slice()), ".", b64(cert.serial_number().as_slice()));
        }

        inline async::task<expected<renewal_info, io::error>> co_renewal_info(tracked_ptr<ClientState> s, crypto::x509::certificate cert) noexcept {
            const char* op = "acme renewal-info";
            auto d = co_await co_directory(s);
            if (!d) {
                co_return unexpected(d.error());
            }
            if (d->renewal_info.empty()) {
                co_return unexpected(acme_error(errc::unsupported, op, string("the directory has no renewalInfo (RFC 9773)")));
            }
            auto id = renewal_id_of(cert);
            if (!id) {
                co_return unexpected(id.error());
            }
            std::string base(d->renewal_info.view());
            if (base.empty() || base.back() != '/') {
                base += '/';
            }
            auto r = co_await co_get(s, string(base + std::string(id->view())), op);
            if (!r) {
                co_return unexpected(r.error());
            }
            auto j = body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            auto info = with_op(parse_renewal_info(*j), op);
            if (info) {
                info->retry_after = r->retry_after;
            }
            co_return info;
        }

        // The reverse-DNS name of an address (RFC 8738 §6): tls-alpn-01's SNI
        inline string reverse_name(const net::ip_address& a) {
            auto b = a.bytes();
            std::string out;
            if (a.is_v4()) {
                for (int i = 15; i >= 12; --i) {
                    out += std::to_string(int(b[i])) + ".";
                }
                out += "in-addr.arpa";
            } else {
                static const char hex[] = "0123456789abcdef";
                for (int i = 15; i >= 0; --i) {
                    out += hex[b[i] & 15];
                    out += '.';
                    out += hex[b[i] >> 4];
                    out += '.';
                }
                out += "ip6.arpa";
            }
            return string(out);
        }

        // The certificate of tls-alpn-01 (RFC 8737 §3): self-signed, for the
        // name alone (an IP address's as its iPAddress, RFC 8738 §6), the
        // acmeIdentifier extension critical with the SHA-256 of the key
        // authorization, on a new P-256 key
        inline expected<tls::identity, io::error> alpn_identity(const string& key_authorization, const string& name) {
            auto key = crypto::p256::private_key::generate();
            crypto::x509::certificate_template t;
            t.common_name = string();
            if (auto a = net::ip_address::parse(name)) {
                crypto::x509::ip_address ip;
                auto b = a->bytes();
                const size_t from = a->is_v4() ? 12 : 0;
                ip.size = uint8_t(16 - from);
                for (size_t i = from; i < 16; ++i) {
                    ip.bytes[i - from] = byte(b[i]);
                }
                t.ip_addresses.push_back(ip);
            } else {
                t.dns_names.push_back(name);
            }
            auto now = time::now();
            t.not_before = now - 24 * hour;
            t.not_after = now + 7 * 24 * hour;
            auto h = crypto::sha256::of(bytes_of(key_authorization.view()));
            crypto::x509::extension e;
            e.oid = string("1.3.6.1.5.5.7.1.31");   // id-pe-acmeIdentifier
            e.critical = true;
            e.value.push_back(byte(0x04));
            e.value.push_back(byte(0x20));
            for (auto b : h) {
                e.value.push_back(b);
            }
            t.extensions.push_back(std::move(e));
            crypto::x509::certificate cert = crypto::x509::create_certificate(t, key);
            string pem = encoding::pem(string("CERTIFICATE"), vector<byte>(cert.raw().begin(), cert.raw().end())).to_string();
            auto key_pem = key.to_pem();
            return tls::identity::from_pem(pem, key_pem.as_slice());
        }
    }

    // An ACME client of one CA and one account key: a handle of one word,
    // its copies the same client (the directory, the account's URL, the
    // nonces), safe from many threads and tasks at once
    class client {
    public:
        // How the client talks to the CA
        struct options {
            net::http::client http;                             // the requests' HTTP client: its TLS roots, proxy, timeouts
            string user_agent;                                  // put before "sgcl-acme/1.0" in User-Agent (RFC 8555 §6.1)
            string account_url;                                 // the account's URL (kid) when known; empty: looked up by the key
            int bad_nonce_retries = 5;                          // a request sent again on badNonce, at most
            duration max_retry_after = std::chrono::seconds(60);   // a rate limit's or a 503's Retry-After waited for at most; longer: the error
            duration poll_interval = std::chrono::seconds(1);   // between two polls when the CA gives no Retry-After
            duration poll_timeout = std::chrono::minutes(3);    // a wait past it: ETIMEDOUT
        };

        // A client of the CA whose directory is at the URL, signing with the
        // key; nothing is sent until the first call
        client(const string& directory_url, const account_key& key)
        : client(directory_url, key, options()) {
        }

        client(const string& directory_url, const account_key& key, const options& o)
        : _s(make_tracked<detail::ClientState>(directory_url, key)) {
            _s->http = o.http;
            _s->user_agent = o.user_agent.empty() ? string("sgcl-acme/1.0") : string::concat(o.user_agent, " sgcl-acme/1.0");
            _s->kid = o.account_url;
            _s->bad_nonce_retries = o.bad_nonce_retries < 0 ? 0 : o.bad_nonce_retries;
            _s->max_retry_after = o.max_retry_after;
            _s->poll_interval = o.poll_interval;
            _s->poll_timeout = o.poll_timeout;
        }

        // The key requests are signed with (a new one after change_key)
        SGCL_INLINE_HOT account_key key() const noexcept {
            return _s->current_key();
        }

        // The account's URL, its kid; empty before the account is known
        SGCL_INLINE_HOT string account_url() const noexcept {
            return _s->current_kid();
        }

        SGCL_INLINE_HOT string directory_url() const noexcept {
            return _s->directory_url;
        }

        // The directory (§7.1.1), asked for once
        // `directory()` on this thread, `co_await async_directory()` in a task
        SGCL_INLINE_HOT expected<acme::directory, io::error> directory() const {
            return async_directory().wait();
        }

        SGCL_INLINE_HOT async::task<expected<acme::directory, io::error>> async_directory() const noexcept {
            return detail::co_directory(_s);
        }

        // A new account of the key (§7.3), or the one it has (the CA answers
        // with the existing account): its URL becomes the client's kid
        expected<acme::account, io::error> register_account(const account_options& o = {}) const {
            return async_register_account(o).wait();
        }

        async::task<expected<acme::account, io::error>> async_register_account(account_options o = {}) const noexcept {
            return detail::co_register(_s, std::move(o));
        }

        // The account as the CA has it now (a POST-as-GET of its URL, the URL
        // looked up by the key first when unknown)
        expected<acme::account, io::error> account() const {
            return async_account().wait();
        }

        async::task<expected<acme::account, io::error>> async_account() const noexcept {
            return detail::co_account_post(_s, string(), "acme account");
        }

        // The account's contact replaced (§7.3.2)
        expected<acme::account, io::error> update_account(const vector<string>& contact) const {
            return async_update_account(contact).wait();
        }

        async::task<expected<acme::account, io::error>> async_update_account(vector<string> contact) const noexcept {
            return detail::co_account_post(_s, detail::json::object({{"contact", detail::strings_json(contact)}}).to_string(), "acme update-account");
        }

        // The account deactivated (§7.3.6): nothing it signs is taken again
        expected<acme::account, io::error> deactivate_account() const {
            return async_deactivate_account().wait();
        }

        async::task<expected<acme::account, io::error>> async_deactivate_account() const noexcept {
            return detail::co_account_post(_s, string("{\"status\":\"deactivated\"}"), "acme deactivate-account");
        }

        // The account's key rolled over to `next` (§7.3.5); the client signs
        // with it from then on
        expected<void, io::error> change_key(const account_key& next) const {
            return async_change_key(next).wait();
        }

        async::task<expected<void, io::error>> async_change_key(account_key next) const noexcept {
            return detail::co_change_key(_s, std::move(next));
        }

        // A new order of the names (§7.4): DNS names, wildcards
        // ("*.example.com"), IP addresses (RFC 8738); a name in Unicode
        // goes in A-labels
        expected<acme::order, io::error> new_order(const vector<string>& names, const order_options& o = {}) const {
            return async_new_order(names, o).wait();
        }

        async::task<expected<acme::order, io::error>> async_new_order(vector<string> names, order_options o = {}) const noexcept {
            return detail::co_new_order(_s, std::move(names), std::move(o));
        }

        // The order of a URL as it is now
        expected<acme::order, io::error> order(const string& url) const {
            return async_order(url).wait();
        }

        async::task<expected<acme::order, io::error>> async_order(string url) const noexcept {
            return detail::co_get_order(_s, std::move(url));
        }

        // The order polled until it is ready or valid (errc::order_invalid
        // for one that ends invalid; ETIMEDOUT past options::poll_timeout)
        expected<acme::order, io::error> wait_order(const string& url) const {
            return async_wait_order(url).wait();
        }

        async::task<expected<acme::order, io::error>> async_wait_order(string url) const noexcept {
            return detail::co_wait_order(_s, std::move(url));
        }

        expected<acme::authorization, io::error> authorization(const string& url) const {
            return async_authorization(url).wait();
        }

        async::task<expected<acme::authorization, io::error>> async_authorization(string url) const noexcept {
            return detail::co_get_authorization(_s, std::move(url));
        }

        // The authorization polled until it is no longer pending: valid, or
        // errc::authorization_invalid with its challenges' problems
        expected<acme::authorization, io::error> wait_authorization(const string& url) const {
            return async_wait_authorization(url).wait();
        }

        async::task<expected<acme::authorization, io::error>> async_wait_authorization(string url) const noexcept {
            return detail::co_wait_authorization(_s, std::move(url));
        }

        // The authorization deactivated (§7.5.2)
        expected<acme::authorization, io::error> deactivate_authorization(const string& url) const {
            return async_deactivate_authorization(url).wait();
        }

        async::task<expected<acme::authorization, io::error>> async_deactivate_authorization(string url) const noexcept {
            return _co_deactivate_authorization(_s, std::move(url));
        }

        expected<acme::challenge, io::error> challenge(const string& url) const {
            return async_challenge(url).wait();
        }

        async::task<expected<acme::challenge, io::error>> async_challenge(string url) const noexcept {
            return detail::co_challenge_post(_s, std::move(url), string(), "acme challenge");
        }

        // The challenge answered (§7.5.1): the CA is told to validate it now
        // (the record published, the response served)
        expected<acme::challenge, io::error> accept(const acme::challenge& c) const {
            return async_accept(c).wait();
        }

        async::task<expected<acme::challenge, io::error>> async_accept(acme::challenge c) const noexcept {
            return detail::co_challenge_post(_s, c.url, string("{}"), "acme accept");
        }

        // The order finalized with the CSR (§7.4) and polled until valid:
        // its certificate URL set
        expected<acme::order, io::error> finalize(const acme::order& o, const crypto::x509::certificate_request& csr) const {
            return async_finalize(o, csr).wait();
        }

        async::task<expected<acme::order, io::error>> async_finalize(acme::order o, crypto::x509::certificate_request csr) const noexcept {
            return detail::co_finalize(_s, std::move(o), std::move(csr));
        }

        // The chain of a certificate URL (§7.4.2), with the URLs of its
        // alternate chains
        expected<certificate_chain, io::error> certificate(const string& url) const {
            return async_certificate(url).wait();
        }

        async::task<expected<certificate_chain, io::error>> async_certificate(string url) const noexcept {
            return detail::co_certificate(_s, std::move(url));
        }

        // The certificate revoked (§7.6): by the account (1), or by the
        // certificate's own key, the identity's (2)
        expected<void, io::error> revoke(const crypto::x509::certificate& cert, revocation_reason reason = revocation_reason::unspecified) const {
            return async_revoke(cert, reason).wait();
        }

        expected<void, io::error> revoke(const tls::identity& id, revocation_reason reason = revocation_reason::unspecified) const {
            return async_revoke(id, reason).wait();
        }

        async::task<expected<void, io::error>> async_revoke(crypto::x509::certificate cert, revocation_reason reason = revocation_reason::unspecified) const noexcept {
            return detail::co_revoke(_s, std::move(cert), reason, {});
        }

        async::task<expected<void, io::error>> async_revoke(tls::identity id, revocation_reason reason = revocation_reason::unspecified) const noexcept {
            return _co_revoke_by_key(_s, std::move(id), reason);
        }

        // When the CA suggests the certificate be renewed (RFC 9773);
        // errc::unsupported for a CA without renewalInfo
        expected<acme::renewal_info, io::error> renewal_info(const crypto::x509::certificate& cert) const {
            return async_renewal_info(cert).wait();
        }

        async::task<expected<acme::renewal_info, io::error>> async_renewal_info(crypto::x509::certificate cert) const noexcept {
            return detail::co_renewal_info(_s, std::move(cert));
        }

        // The ARI identifier of a certificate (RFC 9773 §4.1), what
        // order_options::replaces takes; errc::malformed for a certificate
        // without an authorityKeyIdentifier
        static expected<string, io::error> renewal_id(const crypto::x509::certificate& cert) noexcept {
            return detail::renewal_id_of(cert);
        }

        // --- the challenges ---------------------------------------------------

        // The key authorization of a token (§8.1): what http-01 serves
        SGCL_INLINE_HOT string key_authorization(const string& token) const {
            return key().key_authorization(token);
        }

        // The path http-01 serves the key authorization at (§8.3)
        SGCL_INLINE_HOT static string http01_path(const string& token) {
            return string::concat("/.well-known/acme-challenge/", token);
        }

        // The TXT record of dns-01 (§8.4): its name for a domain (a
        // wildcard's without "*."), and its value for a token
        SGCL_INLINE_HOT static string dns01_name(const string& domain) {
            std::string_view d = domain.view();
            if (d.size() > 2 && d.substr(0, 2) == "*.") {
                d.remove_prefix(2);
            }
            return string::concat("_acme-challenge.", d);
        }

        string dns01_value(const string& token) const {
            auto ka = key_authorization(token);
            auto h = crypto::sha256::of(detail::bytes_of(ka.view()));
            return detail::b64(detail::bytes_of(h));
        }

        // The identity tls-alpn-01 serves for the name (RFC 8737 §3) to a
        // client offering "acme-tls/1" alone
        expected<tls::identity, io::error> tls_alpn01_identity(const string& token, const string& name) const {
            return detail::alpn_identity(key_authorization(token), name);
        }

    private:
        friend struct sgcl::detail::HandleWord;

        tracked_ptr<detail::ClientState> _s;

        SGCL_INLINE_HOT client(sgcl::detail::FromWord, const tracked_ptr<detail::ClientState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::ClientState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::ClientState>& _handle_word() const noexcept {
            return _s;
        }

        static async::task<expected<acme::authorization, io::error>> _co_deactivate_authorization(tracked_ptr<detail::ClientState> s, string url) noexcept {
            const char* op = "acme deactivate-authorization";
            auto kid = co_await detail::co_kid(s);
            if (!kid) {
                co_return unexpected(kid.error());
            }
            auto r = co_await detail::co_post(s, url, string("{\"status\":\"deactivated\"}"), detail::Signer::account, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            auto j = detail::body_json(*r, op);
            if (!j) {
                co_return unexpected(j.error());
            }
            co_return detail::with_op(detail::parse_authorization(*j, url), op);
        }

        static async::task<expected<void, io::error>> _co_revoke_by_key(tracked_ptr<detail::ClientState> s, tls::identity id, revocation_reason reason) noexcept {
            const auto& st = tls::detail::IdentityAccess::state(id);
            // a KeyState over the identity's key, borrowed: the key is the
            // identity's, kept alive by `id` for as long as the request runs
            auto ks = make_tracked<detail::KeyState>();
            auto copy = std::make_unique<detail::IdentityKey>();
            const auto& k = *st.key;
            if (k.p256) {
                copy->p256.emplace(k.p256->clone());
            } else if (k.p384) {
                copy->p384.emplace(k.p384->clone());
            } else if (k.ed25519) {
                copy->ed25519.emplace(k.ed25519->clone());
            } else {
                copy->rsa.emplace(k.rsa->clone());
            }
            tracked_ptr<const detail::KeyState> state = detail::make_key_state(std::move(copy));
            co_return co_await detail::co_revoke(s, st.certificates[0], reason, state);
        }
    };
}
