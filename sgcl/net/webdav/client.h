//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "server.h"
#include "../error.h"
#include "../http/client.h"
#include "../http/request.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/base64.h"
#include "../../encoding/xml.h"
#include "../../time/datetime.h"

#include <cerrno>
#include <ctime>
#include <string>
#include <string_view>
#include <system_error>

// A WebDAV client (RFC 4918) over an HTTP client: a remote tree listed,
// read, written, made, removed, copied, moved and locked
namespace sgcl::net::webdav {
    namespace detail {
        struct DavClientState {
            std::string base;        // the root's URL, without its trailing "/"
            std::string base_path;   // its path, for the hrefs of answers
            http::client http;
            std::string authorization;
        };

        // A failure's status as the errno a file system would give
        inline io::error dav_status_error(int status, const char* op, const std::string& path) noexcept {
            int e = 0;
            switch (status) {
                case 404: e = ENOENT; break;
                case 409: e = ENOENT; break;      // a parent that is not there
                case 401:
                case 403: e = EACCES; break;
                case 405: e = EEXIST; break;      // MKCOL of what is there
                case 412: e = EEXIST; break;      // Overwrite: F and a destination there
                case 413: e = EFBIG; break;
                case 423: e = EBUSY; break;       // locked
                case 507: e = ENOSPC; break;
                default: break;
            }
            if (e) {
                return io::error(error_code(e, std::system_category()), string(op), string(path));
            }
            return io::error(net::make_error_code(net::errc::http_status), string(op), string(path + " " + std::to_string(status)));
        }

        // RFC 1123's date (getlastmodified) read
        inline optional<time::datetime> dav_parse_date(std::string_view s) noexcept {
            std::tm tm{};
            std::string t(s);
            if (!::strptime(t.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm)) {
                return nullopt;
            }
            return time::datetime::from_unix(int64_t(::timegm(&tm)), time::zone::utc());
        }

        // A multistatus read into resources: each response's href decoded
        // and made relative to the base path, its 200 propstat's properties
        inline expected<vector<resource>, io::error> dav_read_multistatus(const std::string& text, const std::string& base_path, const char* op, const std::string& path) {
            auto doc = encoding::xml::parse(string(text));
            if (!doc || doc->local_name().view() != "multistatus" || doc->namespace_uri().view() != "DAV:") {
                return unexpected(net::detail::net_error(net::errc::http_status, string(op), string(path + ": not a multistatus")));
            }
            vector<resource> out;
            for (auto& r : doc->children()) {
                if (!r.is_element() || r.local_name().view() != "response") {
                    continue;
                }
                resource res;
                std::string href;
                for (auto& c : r.children()) {
                    if (!c.is_element()) {
                        continue;
                    }
                    if (c.local_name().view() == "href") {
                        href = std::string(c.text().view());
                        continue;
                    }
                    if (c.local_name().view() != "propstat") {
                        continue;
                    }
                    std::string status(c.child(string("{DAV:}status")).text().view());
                    if (status.find(" 200") == std::string::npos) {
                        continue;
                    }
                    for (auto& prop : c.child(string("{DAV:}prop")).children()) {
                        if (!prop.is_element() || prop.namespace_uri().view() != "DAV:") {
                            continue;
                        }
                        std::string_view n = prop.local_name().view();
                        if (n == "resourcetype") {
                            res.collection = prop.child(string("{DAV:}collection")).is_element();
                        } else if (n == "getcontentlength") {
                            res.size = std::strtoull(prop.text().c_str(), nullptr, 10);
                        } else if (n == "getlastmodified") {
                            res.modified = dav_parse_date(prop.text().view());
                        } else if (n == "getetag") {
                            res.etag = prop.text();
                        } else if (n == "getcontenttype") {
                            res.content_type = prop.text();
                        }
                    }
                }
                // the href: an absolute URL or a path; decoded, the base path taken off
                if (auto u = net::url::parse(string(href)); u && !u->scheme().empty()) {
                    href = std::string(u->path().view());
                }
                href = net::detail::url_unescape(href);
                if (!base_path.empty() && href.compare(0, base_path.size(), base_path) == 0) {
                    href.erase(0, base_path.size());
                }
                if (href.empty() || href[0] != '/') {
                    href.insert(0, "/");
                }
                if (res.collection && href.back() != '/') {
                    href += '/';
                }
                res.path = string(href);
                out.push_back(std::move(res));
            }
            return out;
        }

        inline const char* DavPropfind =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:propfind xmlns:D=\"DAV:\"><D:prop><D:resourcetype/><D:getcontentlength/><D:getlastmodified/>"
            "<D:getetag/><D:getcontenttype/></D:prop></D:propfind>";
    }

    // The settings of a client
    struct client_options {
        string user;          // HTTP Basic credentials; empty: none (or the URL's)
        string password;
    };

    // A client of a WebDAV server's tree, the URL's path its root ("/" of
    // the calls). Paths name resources under the root ("/docs/a.txt");
    // a failure is the errno a file system would give: ENOENT for a
    // resource or a parent that is not there, EEXIST for one that is,
    // EACCES for a refusal, EBUSY for a locked one; another status is
    // net::errc::http_status. A handle of one word.
    //
    //     net::webdav::client dav("https://files.example.com/dav/", {.user = "alice", .password = "secret"});
    //     for (auto& r : *dav.list("/")) println("{} {}", r.path, r.size);
    class client {
        struct State : detail::DavClientState {
            std::string if_token;
        };

    public:
        client() noexcept = default;

        explicit client(const string& url, const client_options& o = {})
        : client(url, http::client(), o) {
        }

        // The same through the program's HTTP client (its TLS, proxy, timeouts)
        client(const string& url, const http::client& h, const client_options& o = {})
        : _s(make_tracked<State>()) {
            std::string base(url.view());
            std::string user(o.user.view()), password(o.password.view());
            if (auto u = net::url::parse(url)) {
                if (user.empty() && !u->username().empty()) {
                    user = net::detail::url_unescape(u->username().view());
                    password = net::detail::url_unescape(u->password().view());
                    // the credentials out of the URL the requests are sent to
                    size_t at = base.find('@');
                    size_t scheme = base.find("://");
                    if (at != std::string::npos && scheme != std::string::npos) {
                        base.erase(scheme + 3, at - scheme - 2);
                    }
                }
                _s->base_path = std::string(u->path().view());
            }
            while (!base.empty() && base.back() == '/') {
                base.pop_back();
            }
            while (!_s->base_path.empty() && _s->base_path.back() == '/') {
                _s->base_path.pop_back();
            }
            _s->base = base;
            _s->http = h;
            if (!user.empty()) {
                _s->authorization = "Basic " + std::string(encoding::base64::standard.encode(string(user + ":" + password)).view());
            }
        }

        // The members of a collection (PROPFIND, Depth 1), the collection itself left out
        // `list(...)` on this thread, `co_await async_list(...)` in a task
        expected<vector<resource>, io::error> list(const string& path = string("/")) const {
            return async_list(path).wait();
        }

        async::task<expected<vector<resource>, io::error>> async_list(string path = string("/")) const noexcept {
            return _co_list(_s, std::move(path));
        }

        // A resource's properties (PROPFIND, Depth 0)
        // `stat(...)` on this thread, `co_await async_stat(...)` in a task
        expected<resource, io::error> stat(const string& path) const {
            return async_stat(path).wait();
        }

        async::task<expected<resource, io::error>> async_stat(string path) const noexcept {
            return _co_stat(_s, std::move(path));
        }

        // A file's bytes (GET)
        // `read(...)` on this thread, `co_await async_read(...)` in a task
        expected<string, io::error> read(const string& path) const {
            return async_read(path).wait();
        }

        async::task<expected<string, io::error>> async_read(string path) const noexcept {
            return _co_read(_s, std::move(path));
        }

        // A file made or replaced with the bytes (PUT); its parent must exist
        // `write(...)` on this thread, `co_await async_write(...)` in a task
        expected<void, io::error> write(const string& path, const string& data) const {
            return async_write(path, data).wait();
        }

        async::task<expected<void, io::error>> async_write(string path, string data) const noexcept {
            return _co_simple(_s, string("PUT"), std::move(path), std::move(data), string(), string(), "webdav write");
        }

        // A collection made (MKCOL); its parent must exist
        // `mkdir(...)` on this thread, `co_await async_mkdir(...)` in a task
        expected<void, io::error> mkdir(const string& path) const {
            return async_mkdir(path).wait();
        }

        async::task<expected<void, io::error>> async_mkdir(string path) const noexcept {
            return _co_simple(_s, string("MKCOL"), std::move(path), string(), string(), string(), "webdav mkdir");
        }

        // A file or a whole collection removed (DELETE)
        // `remove(...)` on this thread, `co_await async_remove(...)` in a task
        expected<void, io::error> remove(const string& path) const {
            return async_remove(path).wait();
        }

        async::task<expected<void, io::error>> async_remove(string path) const noexcept {
            return _co_simple(_s, string("DELETE"), std::move(path), string(), string(), string(), "webdav remove");
        }

        // A copy of a file or a collection's tree (COPY); with overwrite
        // false, EEXIST for a destination there
        // `copy(...)` on this thread, `co_await async_copy(...)` in a task
        expected<void, io::error> copy(const string& from, const string& to, bool overwrite = true) const {
            return async_copy(from, to, overwrite).wait();
        }

        async::task<expected<void, io::error>> async_copy(string from, string to, bool overwrite = true) const noexcept {
            return _co_simple(_s, string("COPY"), std::move(from), string(), std::move(to), string(overwrite ? "T" : "F"), "webdav copy");
        }

        // A file or a collection moved or renamed (MOVE)
        // `move(...)` on this thread, `co_await async_move(...)` in a task
        expected<void, io::error> move(const string& from, const string& to, bool overwrite = true) const {
            return async_move(from, to, overwrite).wait();
        }

        async::task<expected<void, io::error>> async_move(string from, string to, bool overwrite = true) const noexcept {
            return _co_simple(_s, string("MOVE"), std::move(from), string(), std::move(to), string(overwrite ? "T" : "F"), "webdav move");
        }

        // An exclusive write lock (LOCK) for the timeout: its token, which
        // write, remove and move of the resource then need (if_token) and
        // unlock takes
        // `lock(...)` on this thread, `co_await async_lock(...)` in a task
        expected<string, io::error> lock(const string& path, duration timeout = std::chrono::hours(1)) const {
            return async_lock(path, timeout).wait();
        }

        async::task<expected<string, io::error>> async_lock(string path, duration timeout = std::chrono::hours(1)) const noexcept {
            return _co_lock(_s, std::move(path), timeout);
        }

        // The lock of the token taken off (UNLOCK)
        // `unlock(...)` on this thread, `co_await async_unlock(...)` in a task
        expected<void, io::error> unlock(const string& path, const string& token) const {
            return async_unlock(path, token).wait();
        }

        async::task<expected<void, io::error>> async_unlock(string path, string token) const noexcept {
            return _co_simple(_s, string("UNLOCK"), std::move(path), string(), string(), string(), "webdav unlock", string::concat("<", token, ">"));
        }

        // The token of a lock the calls that change a resource present (the
        // If header); empty: none
        void if_token(const string& token) const {
            _s->if_token = token.empty() ? std::string() : "(<" + std::string(token.view()) + ">)";
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        static std::string _url(const State& s, const string& path) {
            std::string p(path.view());
            if (p.empty() || p[0] != '/') {
                p.insert(0, "/");
            }
            return s.base + detail::dav_escape(p);
        }

        static http::request _request(const State& s, const char* method, const string& path) {
            http::request r(string(method), string(_url(s, path)));
            if (!s.authorization.empty()) {
                r.set_header(string("Authorization"), string(s.authorization));
            }
            if (!s.if_token.empty()) {
                r.set_header(string("If"), string(s.if_token));
            }
            return r;
        }

        static async::task<expected<vector<resource>, io::error>> _co_propfind(tracked_ptr<State> s, string path, const char* depth, const char* op) noexcept {
            auto r = _request(*s, "PROPFIND", path);
            r.set_header(string("Depth"), string(depth));
            r.set_header(string("Content-Type"), string("application/xml; charset=utf-8"));
            r.set_body(string(detail::DavPropfind));
            auto res = co_await s->http.async_send(r);
            if (!res) {
                co_return unexpected(res.error());
            }
            auto text = co_await res->async_text();
            if (!text) {
                co_return unexpected(text.error());
            }
            if (res->status() != 207) {
                co_return unexpected(detail::dav_status_error(res->status(), op, std::string(path.view())));
            }
            co_return detail::dav_read_multistatus(std::string(text->view()), s->base_path, op, std::string(path.view()));
        }

        static async::task<expected<vector<resource>, io::error>> _co_list(tracked_ptr<State> s, string path) noexcept {
            std::string p(path.view());
            if (p.empty() || p.back() != '/') {
                p += '/';
            }
            auto all = co_await _co_propfind(s, string(p), "1", "webdav list");
            if (!all) {
                co_return unexpected(all.error());
            }
            vector<resource> out;
            std::string self = p[0] == '/' ? p : "/" + p;
            for (auto& r : *all) {
                std::string rp(r.path.view());
                if (rp == self || rp + "/" == self || rp == self.substr(0, self.size() - 1)) {
                    continue;   // the collection itself
                }
                out.push_back(r);
            }
            co_return out;
        }

        static async::task<expected<resource, io::error>> _co_stat(tracked_ptr<State> s, string path) noexcept {
            auto all = co_await _co_propfind(s, path, "0", "webdav stat");
            if (!all) {
                co_return unexpected(all.error());
            }
            if (all->empty()) {
                co_return unexpected(detail::dav_status_error(404, "webdav stat", std::string(path.view())));
            }
            co_return (*all)[0];
        }

        static async::task<expected<string, io::error>> _co_read(tracked_ptr<State> s, string path) noexcept {
            auto res = co_await s->http.async_send(_request(*s, "GET", path));
            if (!res) {
                co_return unexpected(res.error());
            }
            auto text = co_await res->async_text();
            if (!text) {
                co_return unexpected(text.error());
            }
            if (res->status() != 200) {
                co_return unexpected(detail::dav_status_error(res->status(), "webdav read", std::string(path.view())));
            }
            co_return std::move(*text);
        }

        static async::task<expected<void, io::error>> _co_simple(tracked_ptr<State> s, string method, string path, string body, string destination, string overwrite,
                                                                 const char* op, string lock_token = string()) noexcept {
            auto r = _request(*s, method.c_str(), path);
            if (!destination.empty()) {
                r.set_header(string("Destination"), string(_url(*s, destination)));
                r.set_header(string("Overwrite"), overwrite);
                if (method == "COPY" || method == "MOVE") {
                    r.set_header(string("Depth"), string("infinity"));
                }
            }
            if (!lock_token.empty()) {
                r.set_header(string("Lock-Token"), lock_token);
            }
            if (method == "PUT") {
                r.set_body(body);
            }
            auto res = co_await s->http.async_send(r);
            if (!res) {
                co_return unexpected(res.error());
            }
            (void)co_await res->async_text();
            int st = res->status();
            if (st < 200 || st > 299 || st == 207) {
                co_return unexpected(detail::dav_status_error(st, op, std::string(path.view())));
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<string, io::error>> _co_lock(tracked_ptr<State> s, string path, duration timeout) noexcept {
            auto r = _request(*s, "LOCK", path);
            int64_t secs = timeout.milliseconds() / 1000;
            r.set_header(string("Timeout"), string(secs > 0 ? "Second-" + std::to_string(secs) : std::string("Infinite")));
            r.set_header(string("Depth"), string("0"));
            r.set_header(string("Content-Type"), string("application/xml; charset=utf-8"));
            r.set_body(string("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:lockinfo xmlns:D=\"DAV:\"><D:lockscope><D:exclusive/></D:lockscope>"
                              "<D:locktype><D:write/></D:locktype><D:owner>sgcl</D:owner></D:lockinfo>"));
            auto res = co_await s->http.async_send(r);
            if (!res) {
                co_return unexpected(res.error());
            }
            (void)co_await res->async_text();
            if (res->status() != 200 && res->status() != 201) {
                co_return unexpected(detail::dav_status_error(res->status(), "webdav lock", std::string(path.view())));
            }
            std::string token(res->header(string("Lock-Token")).view());
            if (token.size() >= 2 && token.front() == '<' && token.back() == '>') {
                token = token.substr(1, token.size() - 2);
            }
            if (token.empty()) {
                co_return unexpected(net::detail::net_error(net::errc::http_status, "webdav lock", string(std::string(path.view()) + ": no Lock-Token")));
            }
            co_return string(token);
        }

        tracked_ptr<State> _s;
    };
}
