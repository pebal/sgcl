//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "client.h"
#include "request.h"
#include "response.h"
#include "status.h"
#include "../error.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/function.h"
#include "../../core/rooted.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../encoding/json.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../io/stream.h"

#include <cstdint>

// A file downloaded in one line, and the response's readers that need JSON
// and files: net::http::download(url, path), client::download, and
// response::json, json<T>, save.
namespace sgcl::net::http {
    namespace detail {
        // The client of net::http::download: one for the process, made at the
        // first download (Go's http.DefaultClient)
        inline const client& default_client_instance() noexcept {
            static const rooted<client> shared(std::in_place);
            return *shared;
        }

        // The response's body into the file, through path + ".part" renamed
        // over path at the end: the bytes written, or the first error (the
        // part removed, the file at path as it was). The buffer is managed:
        // a file's write may run on the blocking pool
        inline async::task<expected<uint64_t, io::error>> save_body(response r, string path) noexcept {
            const string part = string::concat(path, ".part");
            auto f = co_await io::async_create(part);
            if (!f) {
                r.close();
                co_return unexpected(f.error());
            }
            io::reader body = r.body();
            vector<byte> buffer(32768);
            uint64_t done = 0;
            for (;;) {
                auto n = co_await body.async_read(buffer.as_slice());
                if (!n) {
                    (void)f->close();
                    (void)io::remove(part);
                    co_return unexpected(n.error());
                }
                if (*n == 0) {
                    break;
                }
                auto w = co_await f->async_write(buffer.as_slice().first(*n));
                if (!w) {
                    r.close();
                    (void)f->close();
                    (void)io::remove(part);
                    co_return unexpected(w.error());
                }
                done += *n;
            }
            if (auto c = f->close(); !c) {
                (void)io::remove(part);
                co_return unexpected(c.error());
            }
            if (auto m = io::rename(part, path); !m) {
                (void)io::remove(part);
                co_return unexpected(m.error());
            }
            co_return done;
        }

        // GET of url through the client, a 2xx saved to path
        inline async::task<expected<response, io::error>> download(client c, string url, string path) noexcept {
            auto r = co_await c.async_get(url);
            if (!r) {
                co_return r;
            }
            if (!r->ok()) {
                r->close();
                const int s = r->status();
                const char code[4] = {char('0' + s / 100 % 10), char('0' + s / 10 % 10), char('0' + s % 10), 0};
                co_return unexpected(net::detail::net_error(net::errc::http_status, "GET", string::concat(url, " (", code, " ", reason(s), ")")));
            }
            auto saved = co_await save_body(*r, std::move(path));
            if (!saved) {
                co_return unexpected(saved.error());
            }
            co_return r;
        }
    }

    inline expected<response, io::error> client::download(const string& url, const string& path) const {
        return async_download(url, path).wait();
    }

    inline async::task<expected<response, io::error>> client::async_download(string url, string path) const noexcept {
        return detail::download(*this, std::move(url), std::move(path));
    }

    // The file at url saved to path through a client of the process's (curl
    // -fo path url): the body streamed through path + ".part", a status
    // other than 2xx the error net::errc::http_status and no file; the
    // response, its body read. From a thread; a task writes
    // `co_await net::http::async_download(url, path)`
    inline expected<response, io::error> download(const string& url, const string& path) {
        return detail::default_client_instance().download(url, path);
    }

    inline async::task<expected<response, io::error>> async_download(string url, string path) noexcept {
        return detail::default_client_instance().async_download(std::move(url), std::move(path));
    }

    // --- response: the readers that need JSON and files ---

    inline expected<encoding::json, io::error> response::json() const {
        return async_json().wait();
    }

    inline async::task<expected<encoding::json, io::error>> response::async_json() const noexcept {
        auto text = co_await async_text();
        if (!text) {
            co_return unexpected(text.error());
        }
        auto v = encoding::json::parse(*text);
        if (!v) {
            co_return unexpected(encoding::detail::to_io_error(v.error(), "json"));
        }
        co_return std::move(*v);
    }

    template<class T>
    expected<T, io::error> response::json() const {
        return async_json<T>().wait();
    }

    template<class T>
    async::task<expected<T, io::error>> response::async_json() const noexcept {
        auto text = co_await async_text();
        if (!text) {
            co_return unexpected(text.error());
        }
        auto v = encoding::json::parse<T>(*text);
        if (!v) {
            co_return unexpected(encoding::detail::to_io_error(v.error(), "json"));
        }
        co_return std::move(*v);
    }

    inline expected<uint64_t, io::error> response::save(const string& path) const {
        return async_save(path).wait();
    }

    inline async::task<expected<uint64_t, io::error>> response::async_save(string path) const noexcept {
        return detail::save_body(*this, std::move(path));
    }
}
