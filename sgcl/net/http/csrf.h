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
#include "session.h"
#include "status.h"
#include "detail/server_state.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/constant_time.h"
#include "../../crypto/hmac.h"
#include "../../crypto/random.h"
#include "../../crypto/secret.h"
#include "../../crypto/sha256.h"
#include "../../encoding/base64.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// Cross-site request forgery refused (net::http::csrf, a middleware):
// unsafe requests from another origin, by the Fetch metadata a browser
// sends (Sec-Fetch-Site) or else its Origin against the Host, as Go's
// http.CrossOriginProtection; and, when asked, a token a form or a script
// sends back, kept in the session (the synchronizer token) or in a signed
// cookie (the double-submit cookie), as OWASP's cheat sheet describes them.
namespace sgcl::net::http {
    namespace detail {
        struct CsrfState {
            uint8_t kind = 0;                  // csrf::tokens
            vector<string> trusted;            // origins let through as they are serialized
            string header;
            string field;
            string cookie;
            std::unique_ptr<crypto::secret_bytes> key;   // double_submit's HMAC key, in plain memory
            function<void(const request&, response_writer&)> deny;

            SGCL_INLINE_HOT bool trusted_origin(std::string_view origin) const noexcept {
                for (auto& t : trusted) {
                    if (t.view() == origin) {
                        return true;
                    }
                }
                return false;
            }
        };

        // What a request's csrf keeps for csrf::token: the middleware's
        // state, the session (synchronizer), the writer (a new cookie of
        // double_submit) and the token once made
        struct CsrfRequest {
            tracked_ptr<CsrfState> st;
            tracked_ptr<SessionState> session;
            tracked_ptr<WriterImpl> writer;
            string cookie_token;               // the request's cookie, when its signature held
            string token;
        };

        inline constexpr char CsrfKey = 0;
        inline constexpr std::string_view CsrfSessionKey = "_csrf";

        SGCL_INLINE_HOT bool safe_method(std::string_view m) noexcept {
            return m == "GET" || m == "HEAD" || m == "OPTIONS";
        }

        // Go's CrossOriginProtection.Check: Sec-Fetch-Site same-origin or
        // none passes; another value only for a trusted Origin; without it,
        // no Origin passes (not a browser, or one older than 2020), an
        // Origin trusted or of the request's Host passes, any other not
        inline bool same_origin(const CsrfState& st, const RequestImpl& req) {
            auto origin = HeadersAccess::find(req.fields, "origin");
            if (auto site = HeadersAccess::find(req.fields, "sec-fetch-site")) {
                const std::string_view v = trim_ows(*site);
                if (v == "same-origin" || v == "none") {
                    return true;
                }
                return origin && st.trusted_origin(trim_ows(*origin));
            }
            if (!origin) {
                return true;
            }
            const std::string_view o = trim_ows(*origin);
            if (st.trusted_origin(o)) {
                return true;
            }
            const size_t scheme = o.find("://");
            if (scheme == std::string_view::npos) {
                return false;   // "null", or not an origin
            }
            std::string_view host = req.dispatch_host;
            if (host.empty()) {
                host = HeadersAccess::find(req.fields, "host").value_or(std::string_view());
            }
            return !host.empty() && iequal(o.substr(scheme + 3), trim_ows(host));
        }

        // A token of 256 random bits, base64url without padding
        inline string csrf_random() {
            byte raw[32];
            crypto::random::fill(slice<byte>(raw, sizeof raw));
            return encoding::base64::raw_url.encode(slice<const byte>(raw, sizeof raw));
        }

        // double_submit's cookie value: the random part and its HMAC-SHA256
        // under the key, "random.mac", as OWASP's signed double-submit
        inline string csrf_signed(const CsrfState& st, const string& random) {
            auto mac = crypto::hmac<crypto::sha256>::of(slice<const byte>(reinterpret_cast<const byte*>(random.data()), random.size()), st.key->as_slice());
            return string::concat(random, ".", encoding::base64::raw_url.encode(slice<const byte>(mac.data(), mac.size())));
        }

        // Whether a cookie value is one of ours: its mac compared in
        // constant time with the one made of its random part
        inline bool csrf_signature_holds(const CsrfState& st, std::string_view value) {
            const size_t dot = value.find('.');
            if (dot == std::string_view::npos || dot == 0) {
                return false;
            }
            const string expected = csrf_signed(st, string(value.substr(0, dot)));
            return crypto::constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(expected.data()), expected.size()),
                                                slice<const byte>(reinterpret_cast<const byte*>(value.data()), value.size()));
        }

        SGCL_INLINE_HOT bool csrf_equal(std::string_view a, std::string_view b) noexcept {
            return !a.empty() && crypto::constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(a.data()), a.size()),
                                                              slice<const byte>(reinterpret_cast<const byte*>(b.data()), b.size()));
        }

        inline void csrf_deny(const CsrfState& st, request& r, response_writer& w) {
            if (st.deny) {
                st.deny(r, w);
            } else {
                w.error(status::forbidden);
            }
        }

        // The token an unsafe request must carry: the session's, or the
        // signed cookie's; "" when it has none (and then it is refused)
        inline string csrf_expected(const CsrfRequest& cr) {
            if (cr.st->kind == 1) {
                auto v = cr.session->find(CsrfSessionKey);
                return v ? *v : string();
            }
            return cr.cookie_token;
        }

        // The token in a urlencoded form's field, the body read for it (and
        // kept for form()), then the handler or the refusal
        inline async::task<> csrf_by_form(tracked_ptr<CsrfState> st, Step next, request r, response_writer w, string expected) {
            auto form = co_await r.async_form();
            bool ok = false;
            if (form) {
                RequestAccess::impl(r)->form_read = *form;
                ok = csrf_equal(form->get(st->field).view(), expected.view());
            }
            if (!ok) {
                csrf_deny(*st, r, w);
                co_return;
            }
            if (auto t = next(r, w)) {
                co_await *t;
            }
        }
    }

    // Cross-site request forgery refused, a middleware (server::use): an
    // unsafe request (not GET, HEAD or OPTIONS) from another origin is
    // answered 403 — by Sec-Fetch-Site, which every browser sends since
    // 2023, or else by its Origin against the Host, as Go's
    // http.CrossOriginProtection; a request with neither passes (not a
    // browser). With tokens, an unsafe request must also send back the token
    // csrf::token gave its page, in a field or a form's field: kept in the
    // session (synchronizer; a sessions middleware before this one) or in a
    // signed cookie (double_submit). Tokens compared in constant time.
    // A handle of one word: copies share the settings
    class csrf {
    public:
        enum class tokens : uint8_t {
            none,            // the origin check alone
            synchronizer,    // a token of the session's, sent back
            double_submit,   // a token of a signed cookie, sent back
        };

        struct options {
            tokens kind = tokens::none;
            vector<string> trusted_origins;            // cross-origin requests let through ("https://app.example")
            string header = "X-CSRF-Token";            // the field a script sends the token in
            string field = "csrf_token";               // the form field (application/x-www-form-urlencoded: the body read for it, form() gives it after)
            string cookie = "__Host-csrf";             // double_submit's cookie: Secure, HttpOnly, SameSite=Lax, Path=/
            function<void(const request&, response_writer&)> deny;   // a refused request's answer; 403 Forbidden by default
        };

        // The origin check alone
        csrf()
        : csrf(options()) {
        }

        // double_submit's cookies signed under a key drawn now: they do not
        // outlive the process (the constructor with a key for several
        // processes behind one name)
        explicit csrf(const options& o)
        : csrf(crypto::random::secret(32), o) {
        }

        // double_submit's cookies signed under the key given (HMAC-SHA256;
        // 32 bytes or more advised)
        csrf(const crypto::secret_bytes& key, const options& o)
        : _s(make_tracked<detail::CsrfState>()) {
            _s->kind = uint8_t(o.kind);
            _s->trusted = o.trusted_origins;
            _s->header = o.header;
            _s->field = o.field;
            _s->cookie = o.cookie;
            _s->key = std::make_unique<crypto::secret_bytes>(key.clone());
            _s->deny = o.deny;
        }

        // The token of the request, to put in its page's form (a hidden
        // field named as the options' field) or meta tag: the session's
        // (made and kept in it the first time) or the signed cookie's (a new
        // cookie set when the request had none, while the head has not
        // gone); "" with no tokens, or when no csrf middleware ran
        static string token(const request& req) {
            auto cr = detail::RequestAccess::impl(req)->attachment(&detail::CsrfKey).template as<detail::CsrfRequest>();
            if (!cr || cr->st->kind == 0) {
                return string();
            }
            if (!cr->token.empty()) {
                return cr->token;
            }
            if (cr->st->kind == 1) {
                if (auto v = cr->session->find(detail::CsrfSessionKey)) {
                    cr->token = *v;
                } else {
                    cr->token = detail::csrf_random();
                    cr->session->values.push_back(pair<string, string>(string(detail::CsrfSessionKey), cr->token));
                    cr->session->changed = true;
                }
                return cr->token;
            }
            if (!cr->cookie_token.empty()) {
                cr->token = cr->cookie_token;
                return cr->token;
            }
            cr->token = detail::csrf_signed(*cr->st, detail::csrf_random());
            if (!cr->writer->head_sent) {
                cookie c(cr->st->cookie, cr->token);
                c.path = "/";
                c.secure = true;
                c.http_only = true;
                c.same_site = "Lax";
                cr->writer->fields.add("Set-Cookie", c.to_string());
            }
            return cr->token;
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [st = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                auto& req = detail::RequestAccess::impl(r);
                if (st->kind != 0) {
                    tracked_ptr cr = make_tracked<detail::CsrfRequest>();
                    cr->st = st;
                    cr->writer = detail::WriterAccess::impl(w);
                    if (st->kind == 1) {
                        cr->session = req->attachment(&detail::SessionKey).template as<detail::SessionState>();
                        if (!cr->session) {
                            throw logic_error("http::csrf: synchronizer tokens need a sessions middleware before csrf");
                        }
                    } else if (auto c = detail::request_cookie(req->fields, st->cookie.view()); c && detail::csrf_signature_holds(*st, c->view())) {
                        cr->cookie_token = *c;
                    }
                    req->attach(&detail::CsrfKey, cr);
                    if (!detail::safe_method(req->method.view())) {
                        if (!detail::same_origin(*st, *req)) {
                            detail::csrf_deny(*st, r, w);
                            return nullopt;
                        }
                        const string expected = detail::csrf_expected(*cr);
                        if (auto sent = detail::HeadersAccess::find(req->fields, st->header.view())) {
                            if (!detail::csrf_equal(detail::trim_ows(*sent), expected.view())) {
                                detail::csrf_deny(*st, r, w);
                                return nullopt;
                            }
                            return next(r, w);
                        }
                        if (detail::media_type(req->fields.get("Content-Type").view()) == "application/x-www-form-urlencoded" && !expected.empty()) {
                            return detail::csrf_by_form(st, next, r, w, expected);
                        }
                        detail::csrf_deny(*st, r, w);
                        return nullopt;
                    }
                    return next(r, w);
                }
                if (!detail::safe_method(req->method.view()) && !detail::same_origin(*st, *req)) {
                    detail::csrf_deny(*st, r, w);
                    return nullopt;
                }
                return next(r, w);
            };
        }

        tracked_ptr<detail::CsrfState> _s;
    };
}
