//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The client's side of WebSocket: the opening handshake over the HTTP
// client's dial (download.h includes this at its end, after the client
// of the process's that websocket::connect goes through)
namespace sgcl::net::http {
    namespace detail {
        // ws:// and wss:// as the HTTP URL the dial takes (http://,
        // https://); nullopt for another scheme
        inline optional<net::url> ws_http_url(const string& text) noexcept {
            auto u = net::url::parse(text);
            if (!u) {
                return nullopt;
            }
            auto scheme = u->scheme();
            std::string_view rest = text.view().substr(text.view().find(':'));
            if (scheme == "ws") {
                auto h = net::url::parse(string::concat("http", rest));
                return h ? optional<net::url>(std::move(*h)) : nullopt;
            }
            if (scheme == "wss") {
                auto h = net::url::parse(string::concat("https", rest));
                return h ? optional<net::url>(std::move(*h)) : nullopt;
            }
            if (scheme == "http" || scheme == "https") {
                return *u;
            }
            return nullopt;
        }

        // A client's opening handshake over the client's dial (its proxy as
        // a tunnel, its TLS with ALPN http/1.1, its dial function), within
        // the handshake's timeout
        inline async::task<expected<websocket, io::error>> ws_connect(tracked_ptr<ClientSettings> cfg, string url, websocket::options o) noexcept {
            auto target = ws_http_url(url);
            auto fail = [&](error_code code, std::string why) {
                return io::error(code, "GET", why.empty() ? url : string::concat(url, " (", why, ')'));
            };
            if (!target) {
                co_return unexpected(fail(net::url::parse(url) ? make_error_code(net::errc::unsupported_scheme) : make_error_code(net::errc::invalid_url), ""));
            }
            if (o.stop.stop_requested()) {
                co_return unexpected(fail(error_code(ECANCELED, std::system_category()), ""));
            }
            Outgoing out;
            out.method = "GET";
            out.target = target->without_fragment();
            out.fields = o.headers;
            if (cfg->jar) {
                // the client's jar: its cookies with the handshake, as a browser sends them
                if (auto fields = with_jar(*cfg->jar, out, vector<string>())) {
                    out.fields = std::move(*fields);
                }
            }
            if (auto e = unsendable(out)) {
                co_return unexpected(*e);
            }
            auto routed = route_for(*cfg, out);
            if (!routed) {
                co_return unexpected(routed.error());
            }
            Route route = *routed;
            if (route.kind == Route::Kind::forward) {
                route.kind = Route::Kind::tunnel;   // an upgrade goes through a tunnel, never forwarded
            }
            const time_point deadline = o.handshake_timeout > duration::zero() ? sgcl::clock::now() + o.handshake_timeout : time_point();
            auto dialed = co_await dial_origin(cfg, out, deadline, route);
            if (!dialed) {
                co_return unexpected(route.kind == Route::Kind::direct ? client_error(dialed.error(), out) : dialed.error());
            }
            net::connection c = *dialed;
            c.set_deadline(deadline);
            // the request (§4.1)
            const string key = ws_key();
            std::string req;
            req.reserve(256);
            req += "GET ";
            req += out.target->request_target().view();
            req += " HTTP/1.1\r\n";
            if (!HeadersAccess::count(out.fields, "host")) {
                req += "Host: ";
                req += out.target->host().view();
                req += "\r\n";
            }
            req += "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ";
            req += key.view();
            req += "\r\n";
            std::vector<std::string> protocols;
            if (!o.subprotocols.empty()) {
                req += "Sec-WebSocket-Protocol: ";
                for (size_t i = 0; i < o.subprotocols.size(); ++i) {
                    if (i) {
                        req += ", ";
                    }
                    req += o.subprotocols[i].view();
                    protocols.emplace_back(o.subprotocols[i].view());
                }
                req += "\r\n";
            }
            if (o.compression) {
                req += "Sec-WebSocket-Extensions: permessage-deflate\r\n";
            }
            for (auto& f : HeadersAccess::fields(out.fields)) {
                auto n = f.first.view();
                if (iequal(n, "upgrade") || iequal(n, "connection") || iequal(n, "sec-websocket-key") || iequal(n, "sec-websocket-version")
                    || iequal(n, "sec-websocket-protocol") || iequal(n, "sec-websocket-extensions")) {
                    continue;
                }
                req += n;
                req += ": ";
                req += f.second.view();
                req += "\r\n";
            }
            req += "\r\n";
            auto w = co_await c.async_write(string(std::string_view(req)));
            if (!w) {
                (void)c.close();
                co_return unexpected(client_error(w.error(), out));
            }
            tracked_ptr wire = make_tracked<Wire>(c);
            auto head = co_await wire->read_head(cfg->max_response_header_bytes);
            if (!head || !*head) {
                (void)c.close();
                co_return unexpected(client_error(head ? io::error(io::errc::unexpected_eof, "read", "") : head.error(), out));
            }
            StatusLine line;
            headers fields;
            string text = **head;
            if (parse_response_head(text, line, fields)) {
                (void)c.close();
                co_return unexpected(client_error(net::errc::malformed_response, out));
            }
            if (cfg->jar && HeadersAccess::count(fields, "set-cookie")) {
                vector<cookie> set;
                for (auto& f : HeadersAccess::fields(fields)) {
                    if (iequal(f.first.view(), "set-cookie")) {
                        if (auto c = parse_cookie(string(f.second.view()))) {
                            set.push_back(std::move(*c));
                        }
                    }
                }
                cfg->jar->set_cookies(*out.target, set);
            }
            WsAnswer answer;
            if (const char* why = check_ws_answer(line.status, fields, key.view(), protocols, o.compression, answer)) {
                (void)c.close();
                co_return unexpected(fail(make_error_code(net::errc::websocket_handshake), std::to_string(line.status) + ": " + why));
            }
            c.set_deadline(time_point());
            std::string rest(wire->view());
            co_return WebSocketAccess::make(c, true, rest, o, answer.subprotocol, answer.deflate);
        }

        // A client's settings for a WebSocket: ALPN http/1.1 alone
        SGCL_INLINE_HOT tracked_ptr<ClientSettings> ws_settings(tracked_ptr<ClientSettings> cfg) noexcept {
            cfg->http2 = false;
            vector<string> alpn;
            for (auto& p : cfg->tls.alpn) {
                if (p != "h2") {
                    alpn.push_back(p);
                }
            }
            if (alpn.empty()) {
                alpn.push_back(string("http/1.1"));
            }
            cfg->tls.alpn = std::move(alpn);
            return cfg;
        }
    }

    inline expected<websocket, io::error> websocket::connect(const string& url) {
        return detail::default_client_instance().websocket(url);
    }

    inline expected<websocket, io::error> websocket::connect(const string& url, const options& o) {
        return detail::default_client_instance().websocket(url, o);
    }

    inline async::task<expected<websocket, io::error>> websocket::async_connect(string url) noexcept {
        return detail::default_client_instance().async_websocket(std::move(url));
    }

    inline async::task<expected<websocket, io::error>> websocket::async_connect(string url, options o) noexcept {
        return detail::default_client_instance().async_websocket(std::move(url), std::move(o));
    }

    inline expected<websocket, io::error> client::websocket(const string& url) const {
        return async_websocket(url).wait();
    }

    inline expected<http::websocket, io::error> client::websocket(const string& url, const http::websocket::options& o) const {
        return async_websocket(url, o).wait();
    }

    inline async::task<expected<http::websocket, io::error>> client::async_websocket(string url) const noexcept {
        return detail::ws_connect(detail::ws_settings(_settings()), std::move(url), http::websocket::options());
    }

    inline async::task<expected<http::websocket, io::error>> client::async_websocket(string url, http::websocket::options o) const noexcept {
        return detail::ws_connect(detail::ws_settings(_settings()), std::move(url), std::move(o));
    }
}
