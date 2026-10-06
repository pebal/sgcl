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
#include "detail/server_state.h"
#include "../../async/coroutine.h"
#include "../../async/rate_limiter.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/map.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../slog/logger.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <mutex>
#include <string>
#include <string_view>

// The middlewares of the server that need nothing but the module and slog
// (server::use, or m.wrap(handler) for one route): cors (the Fetch
// standard's CORS protocol), recovery (a handler's exception logged and
// answered), request_log (a record of each exchange through slog) and
// body_limit. The shape they share (detail::Step, handler) is in
// detail/server_state.h; sessions, csrf, rate_limit, the authentications
// and compression have headers of their own.
namespace sgcl::net::http {
    namespace detail {
        // What a cors middleware decided once, at its construction: the
        // lists joined as they are sent
        struct CorsState {
            vector<string> origins;            // as given, compared exactly
            bool any = false;                  // "*" among them
            function<bool(const string&)> allow_origin;
            vector<string> methods;
            string methods_joined;
            vector<string> headers;            // lower case
            string headers_joined;
            string exposed_joined;
            bool credentials = false;
            string max_age;                    // seconds, "" for none

            SGCL_INLINE_HOT bool allowed(std::string_view origin) const {
                if (allow_origin) {
                    return allow_origin(string(origin));
                }
                for (auto& o : origins) {
                    if (o.view() == origin) {
                        return true;
                    }
                }
                return any && origin != "null";   // "*" is every origin but the opaque one, which must be named
            }

            SGCL_INLINE_HOT bool method_allowed(std::string_view m) const noexcept {
                if (m == "GET" || m == "HEAD" || m == "POST") {
                    return true;   // CORS-safelisted methods (Fetch §2.2.1)
                }
                for (auto& x : methods) {
                    if (x.view() == m) {
                        return true;
                    }
                }
                return false;
            }
        };

        // A list of a field ("a, b" with OWS) joined into one string
        inline string join_list(const vector<string>& items, bool lower) {
            std::string out;
            for (auto& s : items) {
                if (!out.empty()) {
                    out += ", ";
                }
                for (char c : s.view()) {
                    out += lower ? ascii_lower(c) : c;
                }
            }
            return string(std::string_view(out));
        }

        // Whether every name of a preflight's Access-Control-Request-Headers
        // is allowed: in the list, or a CORS-safelisted request header (Fetch
        // §2.2.2; Content-Type's value is not seen by a preflight, so it is
        // always listed when asked for)
        inline bool headers_allowed(const CorsState& s, std::string_view asked) {
            while (!asked.empty()) {
                const size_t comma = asked.find(',');
                std::string_view name = trim_ows(asked.substr(0, comma));
                asked = comma == std::string_view::npos ? std::string_view() : asked.substr(comma + 1);
                if (name.empty()) {
                    continue;
                }
                if (iequal(name, "accept") || iequal(name, "accept-language") || iequal(name, "content-language")) {
                    continue;
                }
                bool found = false;
                for (auto& h : s.headers) {
                    found = found || iequal(h.view(), name);
                }
                if (!found) {
                    return false;
                }
            }
            return true;
        }

        // The CORS protocol before the handler (Fetch §3.2): a request
        // without Origin is not a CORS request and goes on as it is; a
        // preflight (OPTIONS with Access-Control-Request-Method) is answered
        // here, 204, with the permissions when the origin, the method and the
        // headers are allowed, and with none when not (the browser then
        // refuses the request); an actual request from an allowed origin gets
        // Access-Control-Allow-Origin (the origin echoed, or * for any origin
        // without credentials), -Allow-Credentials and -Expose-Headers.
        // Whether the handler is to run
        inline bool cors_before(const CorsState& s, const RequestImpl& req, WriterImpl& w) {
            const bool echo = !(s.any && !s.credentials && !s.allow_origin && s.origins.size() == 1);
            auto origin = HeadersAccess::find(req.fields, "origin");
            const bool preflight = origin && req.method.view() == "OPTIONS" && HeadersAccess::count(req.fields, "access-control-request-method");
            if (preflight) {
                w.fields.add("Vary", "Origin, Access-Control-Request-Method, Access-Control-Request-Headers");
                w.status = status::no_content;
                w.touched = true;
                if (!s.allowed(*origin)) {
                    return false;
                }
                const std::string_view method = trim_ows(*HeadersAccess::find(req.fields, "access-control-request-method"));
                if (!s.method_allowed(method)) {
                    return false;
                }
                auto asked = HeadersAccess::find(req.fields, "access-control-request-headers");
                if (asked && !s.headers.empty() && !headers_allowed(s, *asked)) {
                    return false;
                }
                w.fields.set("Access-Control-Allow-Origin", echo ? string(*origin) : string("*"));
                if (s.credentials) {
                    w.fields.set("Access-Control-Allow-Credentials", "true");
                }
                w.fields.set("Access-Control-Allow-Methods", s.methods_joined);
                if (asked && !trim_ows(*asked).empty()) {
                    w.fields.set("Access-Control-Allow-Headers", s.headers.empty() ? string(trim_ows(*asked)) : s.headers_joined);
                }
                if (!s.max_age.empty()) {
                    w.fields.set("Access-Control-Max-Age", s.max_age);
                }
                return false;
            }
            if (echo) {
                w.fields.add("Vary", "Origin");   // the answer depends on the Origin: a cache keys by it
            }
            if (origin && s.allowed(*origin)) {
                w.fields.set("Access-Control-Allow-Origin", echo ? string(*origin) : string("*"));
                if (s.credentials) {
                    w.fields.set("Access-Control-Allow-Credentials", "true");
                }
                if (!s.exposed_joined.empty()) {
                    w.fields.set("Access-Control-Expose-Headers", s.exposed_joined);
                }
            }
            return true;
        }

        // What a recovery middleware keeps
        struct RecoveryState {
            optional<slog::logger> log;
            function<void(const request&, response_writer&, const string&)> answer;
        };

        // A handler's exception caught: logged at error (method, path,
        // error), and answered — by the program's answer, or a 500 of the
        // server's own fields — when nothing has gone yet; a response whose
        // head has gone is broken off, so that no client takes what went for
        // a whole response, and the connection ends after it
        inline void recovered(const RecoveryState& s, const request& r, response_writer& w, std::string_view what) {
            auto& req = *RequestAccess::impl(r);
            auto& wi = *WriterAccess::impl(w);
            const slog::logger log = s.log ? *s.log : slog::default_logger();
            log.error("handler threw", "method", req.method.view(), "path", req.dispatch_path, "error", what);
            wi.close_after = true;
            if (wi.head_sent || wi.hijacked) {
                WriterAccess::abort(w);
                return;
            }
            wi.fields = http::headers();
            wi.trailers = http::headers();
            if (s.answer) {
                s.answer(r, w, string(what));
            } else {
                w.error(status::internal_server_error);
            }
        }

        inline async::task<> recover_task(tracked_ptr<RecoveryState> s, async::task<> t, request r, response_writer w) {
            std::string what;
            bool threw = false;
            try {
                co_await t;
            } catch (const std::exception& e) {
                what = e.what();
                threw = true;
            } catch (...) {
                what = "an exception not of std::exception";
                threw = true;
            }
            if (threw) {
                recovered(*s, r, w, what);
            }
        }

        // A key's bucket and when it was last asked
        struct RateBucket {
            async::rate_limiter limiter;
            int64_t used_ns = 0;

            SGCL_INLINE_HOT RateBucket(double per_second, size_t burst) noexcept
            : limiter(per_second, burst) {
            }
        };

        // What a rate_limit keeps: its rate, how a key is made, the buckets
        // by key (under a mutex: a request takes it once, for its bucket)
        struct RateLimitState {
            double per_second = 0;
            size_t burst = 1;
            string header;
            function<string(const request&)> key;
            int64_t idle_ns = 0;
            size_t max_keys = 100000;
            std::mutex lock;
            map<string, tracked_ptr<RateBucket>> buckets;
            int64_t swept_ns = 0;

            // The key of a request: the program's, the field's value, or the
            // remote address (without its port)
            string key_of(const request& r) const {
                if (key) {
                    return key(r);
                }
                if (!header.empty()) {
                    return r.header(header);
                }
                return r.remote_endpoint().address().to_string();
            }

            // The key's bucket, made at its first request; the buckets idle
            // past idle_ns dropped now and then, and when the keys pass
            // max_keys the idle first, then the least recently used
            tracked_ptr<RateBucket> bucket(const string& k, int64_t now) {
                std::lock_guard<std::mutex> g(lock);
                auto it = buckets.find(k);
                if (it != buckets.end()) {
                    it->second->used_ns = now;
                    return it->second;
                }
                if (buckets.size() >= max_keys || now - swept_ns > idle_ns) {
                    sweep(now);
                }
                tracked_ptr b = make_tracked<RateBucket>(per_second, burst);
                b->used_ns = now;
                buckets[k] = b;
                return b;
            }

            void sweep(int64_t now) {
                vector<string> gone;
                for (auto& [k, b] : buckets) {
                    if (now - b->used_ns > idle_ns) {
                        gone.push_back(k);
                    }
                }
                for (auto& k : gone) {
                    buckets.erase(k);
                }
                while (buckets.size() >= max_keys && !buckets.empty()) {
                    auto oldest = buckets.begin();
                    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
                        if (it->second->used_ns < oldest->second->used_ns) {
                            oldest = it;
                        }
                    }
                    buckets.erase(oldest);
                }
                swept_ns = now;
            }
        };

        // The record of request_log, once the response has gone: what
        // server::access_log writes
        struct RequestLogState {
            slog::logger log;

            SGCL_INLINE_HOT explicit RequestLogState(const slog::logger& l) noexcept
            : log(l) {
            }
        };
    }

    // Cross-origin requests by the Fetch standard's CORS protocol: a
    // preflight answered (204 with the permissions, or none), an actual
    // request from an allowed origin let read its response
    //
    //     srv.use(net::http::cors({.origins = {"https://app.example"}, .credentials = true}));
    //
    // A handle of one word: copies share the settings
    class cors {
    public:
        struct options {
            vector<string> origins = {string("*")};    // origins allowed as browsers serialize them ("https://a.example:8443"); "*" any but "null"
            function<bool(const string&)> allow_origin;   // a predicate of the program's instead, given the Origin (wins over origins)
            vector<string> methods = {string("GET"), string("HEAD"), string("PUT"), string("PATCH"), string("POST"), string("DELETE")};
            vector<string> headers;                    // request headers allowed; empty: those a preflight asks for
            vector<string> exposed_headers;            // response headers a script may read beyond the safelisted
            bool credentials = false;                  // cookies and Authorization allowed: the origin echoed, never *
            duration max_age = std::chrono::minutes(5);   // how long a browser keeps a preflight's answer; zero: none sent
        };

        // Any origin but "null", the common methods, the headers a
        // preflight asks for, no credentials
        cors()
        : cors(options()) {
        }

        explicit cors(const options& o)
        : _s(make_tracked<detail::CorsState>()) {
            _s->origins = o.origins;
            for (auto& x : o.origins) {
                _s->any = _s->any || x == "*";
            }
            _s->allow_origin = o.allow_origin;
            _s->methods = o.methods;
            _s->methods_joined = detail::join_list(o.methods, false);
            for (auto& h : o.headers) {
                _s->headers.push_back(detail::join_list(vector<string>{h}, true));
            }
            _s->headers_joined = detail::join_list(o.headers, false);
            _s->exposed_joined = detail::join_list(o.exposed_headers, false);
            _s->credentials = o.credentials;
            if (o.max_age > duration::zero()) {
                _s->max_age = string(std::to_string(o.max_age.nanoseconds() / 1000000000));
            }
        }

        // The middleware around one handler: `srv.route("/api/", cors.wrap(api))`
        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [s = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                if (!detail::cors_before(*s, *detail::RequestAccess::impl(r), *detail::WriterAccess::impl(w))) {
                    return nullopt;
                }
                return next(r, w);
            };
        }

        tracked_ptr<detail::CorsState> _s;
    };

    // A handler's exception caught: logged through slog at error (method,
    // path, error) and answered 500 (or as the program's answer says) when
    // nothing has gone yet; broken off when its head has
    //
    //     srv.use(net::http::recovery());
    class recovery {
    public:
        struct options {
            optional<slog::logger> log;                // where the record goes; slog's default logger when none
            function<void(const request&, response_writer&, const string& what)> answer;   // the answer; a 500 text/plain by default
        };

        // slog's default logger, a 500
        recovery()
        : recovery(options()) {
        }

        explicit recovery(const options& o)
        : _s(make_tracked<detail::RecoveryState>()) {
            _s->log = o.log;
            _s->answer = o.answer;
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [s = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                try {
                    auto t = next(r, w);
                    if (!t) {
                        return nullopt;
                    }
                    return detail::recover_task(s, std::move(*t), r, w);
                } catch (const std::exception& e) {
                    detail::recovered(*s, r, w, e.what());
                } catch (...) {
                    detail::recovered(*s, r, w, "an exception not of std::exception");
                }
                return nullopt;
            };
        }

        tracked_ptr<detail::RecoveryState> _s;
    };

    // A record of every exchange through slog once its response has gone:
    // `method`, `path`, `proto`, `status`, `bytes` (of the body), `duration`
    // (from the request's first byte), `remote`, `user_agent`, and
    // `request_id` for an X-Request-ID; at info, at error for a 5xx.
    // server::access_log writes the same for the whole server; this one is
    // for a route or a group of them
    class request_log {
    public:
        explicit request_log(const slog::logger& log = slog::default_logger()) noexcept
        : _s(make_tracked<detail::RequestLogState>(log)) {
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [s = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                auto& wi = *detail::WriterAccess::impl(w);
                detail::add_after_end(wi, [s, req = detail::RequestAccess::impl(r), start = sgcl::clock::now()](const detail::WriterImpl& done, uint64_t bytes) {
                    const std::string_view proto = req->h2 ? "HTTP/2.0" : req->minor == 0 ? "HTTP/1.0" : "HTTP/1.1";
                    detail::log_record(s->log, *req, done, bytes, req->dispatch_path, proto, start);
                });
                return next(r, w);
            };
        }

        tracked_ptr<detail::RequestLogState> _s;
    };

    // The body's limit below the server's max_body_bytes, for one route or a
    // group: a Content-Length past it is answered 413 before the handler
    // runs (and the connection closed); a chunked body read past it is the
    // read's error, net::errc::body_too_large, and 413 when the handler
    // wrote nothing, as with the server's own limit. A limit of 0 takes no
    // body: a chunked one is refused before it is read
    //
    //     srv.route("POST /upload", net::http::body_limit(10 << 20).wrap(upload));
    class body_limit {
    public:
        explicit body_limit(uint64_t max_bytes) noexcept
        : _max(max_bytes) {
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [max = _max, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                auto& req = *detail::RequestAccess::impl(r);
                // past the limit by its length; with a limit of 0, any body
                // but an empty one of a length (a chunked one is refused
                // whole: it cannot be known empty before it is read)
                if ((req.content_length && *req.content_length > max) || (max == 0 && req.body && !req.content_length)) {
                    detail::WriterAccess::impl(w)->close_after = true;   // the body is not read: the connection cannot go on
                    w.error(status::content_too_large);
                    return nullopt;
                }
                if (req.body) {
                    req.body->lower_limit(max);
                }
                return next(r, w);
            };
        }

        uint64_t _max;
    };

    // Requests per key, a middleware: a token bucket per key
    // (async::rate_limiter, per_second tokens a second, burst at once); a
    // request whose key has no token now is answered 429 Too Many Requests
    // with Retry-After, the seconds until it would have one (RFC 6585 §4).
    // The key is the remote address by default, a field's value
    // (`.header = "X-API-Key"`), or the program's function
    //
    //     srv.route("/api/", net::http::rate_limit(10, 20).wrap(api));
    //
    // A handle of one word: copies share the buckets
    class rate_limit {
    public:
        struct options {
            string header;                             // the key is this field's value ("X-API-Key"); "" (a missing field) is one key
            function<string(const request&)> key;      // or the program's, which wins over header; by default the remote address
            duration idle = std::chrono::minutes(10);  // a key's bucket dropped after this long unused (it was full again long before)
            size_t max_keys = 100000;                  // past it the idle buckets dropped, then the least recently used
        };

        // invalid_argument for a burst of 0, or a rate that is not a number
        rate_limit(double per_second, size_t burst)
        : rate_limit(per_second, burst, options()) {
        }

        rate_limit(double per_second, size_t burst, const options& o)
        : _s(make_tracked<detail::RateLimitState>()) {
            if (burst == 0 || per_second != per_second || per_second < 0) {
                throw invalid_argument("http::rate_limit: a burst of 1 or more and a rate of 0 or more");
            }
            _s->per_second = per_second;
            _s->burst = burst;
            _s->header = o.header;
            _s->key = o.key;
            _s->idle_ns = o.idle.nanoseconds() > 0 ? o.idle.nanoseconds() : 1;
            _s->max_keys = o.max_keys ? o.max_keys : 1;
        }

        // The buckets kept now
        size_t keys() const {
            std::lock_guard<std::mutex> g(_s->lock);
            return _s->buckets.size();
        }

        SGCL_INLINE_HOT handler wrap(const handler& next) const {
            return detail::HandlerAccess::make(_wrap(detail::HandlerAccess::step(next)));
        }

    private:
        friend struct detail::MiddlewareAccess;

        detail::Step _wrap(detail::Step next) const {
            return [st = _s, next = std::move(next)](request& r, response_writer& w) -> optional<async::task<>> {
                const int64_t now = sgcl::clock::now().time_since_epoch().count();
                auto b = st->bucket(st->key_of(r), now);
                if (b->limiter.allow()) {
                    return next(r, w);
                }
                // Retry-After: when one token will be there, rounded up to a second
                auto res = b->limiter.reserve();
                int64_t wait = 1;
                if (res.ok()) {
                    wait = (res.delay().nanoseconds() + 999999999) / 1000000000;
                    if (wait < 1) {
                        wait = 1;
                    }
                    res.cancel();
                }
                w.set_header("Retry-After", string(std::to_string(wait)));
                w.error(status::too_many_requests);
                return nullopt;
            };
        }

        tracked_ptr<detail::RateLimitState> _s;
    };
}
