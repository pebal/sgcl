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
#include "detail/content.h"
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

        // The response's body appended to an open file: the bytes written,
        // or the first error, and whether it was the file's (a write) or
        // the body's (a read: the network). The buffer is managed: a
        // file's write may run on the blocking pool
        struct BodyWritten {
            expected<uint64_t, io::error> result;
            bool file_failed = false;
        };

        inline async::task<BodyWritten> write_body(response r, io::file f) noexcept {
            io::reader body = r.body();
            vector<byte> buffer(32768);
            uint64_t done = 0;
            for (;;) {
                auto n = co_await body.async_read(buffer.as_slice());
                if (!n) {
                    co_return BodyWritten{unexpected(n.error()), false};
                }
                if (*n == 0) {
                    co_return BodyWritten{done, false};
                }
                auto w = co_await f.async_write(buffer.as_slice().first(*n));
                if (!w) {
                    r.close();
                    co_return BodyWritten{unexpected(w.error()), true};
                }
                done += *n;
            }
        }

        // The response's body into the file, through path + ".part" renamed
        // over path at the end: the bytes written, or the first error (the
        // part removed, the file at path as it was)
        inline async::task<expected<uint64_t, io::error>> save_body(response r, string path) noexcept {
            const string part = string::concat(path, ".part");
            auto f = co_await io::async_create(part);
            if (!f) {
                r.close();
                co_return unexpected(f.error());
            }
            auto written = co_await write_body(r, *f);
            auto& done = written.result;
            if (!done) {
                (void)f->close();
                (void)io::remove(part);
                co_return unexpected(done.error());
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

        // The status of a response as an error: net::errc::http_status
        // with the method, the URL, the code and its phrase
        inline io::error status_error(const string& url, int s) noexcept {
            const char code[4] = {char('0' + s / 100 % 10), char('0' + s / 10 % 10), char('0' + s % 10), 0};
            return net::detail::net_error(net::errc::http_status, "GET", string::concat(url, " (", code, " ", reason(s), ")"));
        }

        // The validator a resumption's If-Range carries (RFC 9110 §13.1.5):
        // a strong ETag, else the Last-Modified; "" when the response gave
        // neither, and then a download is never continued, since a Range
        // without If-Range would append the bytes of a newer file to the
        // start of an older one
        inline string resume_validator(const response& r) noexcept {
            string etag = r.header("ETag");
            if (!etag.empty() && !etag_weak(etag.view()) && valid_etag(etag.view())) {
                return etag;
            }
            return r.header("Last-Modified");
        }

        // GET of url through the client, a 2xx saved to path. Not resumed
        // (no options): the body through path + ".part", as it always was.
        // With options, the part may be continued: from an earlier call
        // (resume, the URL and the validator read from path + ".part.meta")
        // and within this call (retries, after a body broken off), each
        // time a Range from the part's size with If-Range; a 206 from that
        // byte is appended, a 200 starts the part over, a 416 that says the
        // file is the part's size completes it
        inline async::task<expected<response, io::error>> download(client c, string url, string path, download_options o) noexcept {
            const string part = string::concat(path, ".part");
            const string meta = string::concat(path, ".part.meta");
            uint64_t offset = 0;
            string validator;
            if (o.resume) {
                auto kept = co_await io::async_read_text(meta);
                auto info = co_await io::async_stat(part);
                if (kept && info && info->is_regular()) {
                    auto v = kept->view();
                    const size_t nl = v.find('\n');
                    if (nl != std::string_view::npos && v.substr(0, nl) == url.view()) {
                        auto rest = v.substr(nl + 1);
                        validator = string(rest.substr(0, rest.find('\n')));
                        offset = validator.empty() ? 0 : info->size;
                    }
                }
            }
            if (offset == 0) {
                validator = string();
            }
            // what a failure leaves: with resume and a validator, the part
            // and its meta for the next call; else nothing
            auto give_up = [&](const io::error& e, bool keep) -> expected<response, io::error> {
                if (!(keep && o.resume && !validator.empty())) {
                    (void)io::remove(part);
                    (void)io::remove(meta);
                }
                return unexpected(e);
            };
            int retries = o.retries > 0 ? o.retries : 0;
            bool restarted = false;
            for (;;) {
                request req("GET", url);
                if (offset > 0) {
                    req.set_header("Range", string::concat("bytes=", std::to_string(offset), "-"));
                    req.set_header("If-Range", validator);
                }
                auto r = co_await c.async_send(req);
                if (!r) {
                    if (offset > 0 && retries > 0 && !validator.empty()) {
                        --retries;
                        continue;
                    }
                    co_return give_up(r.error(), true);
                }
                const int st = r->status();
                bool append = false;
                if (offset > 0 && st == status::partial_content) {
                    auto cr = parse_content_range(r->header("Content-Range").view());
                    if (cr && !cr->unsatisfied && cr->first == offset) {
                        append = true;
                    } else {
                        r->close();   // not the bytes after the part: started over, once
                        if (restarted) {
                            co_return give_up(status_error(url, st), false);
                        }
                        restarted = true;
                        offset = 0;
                        validator = string();
                        continue;
                    }
                } else if (offset > 0 && st == status::range_not_satisfiable) {
                    r->close();
                    auto cr = parse_content_range(r->header("Content-Range").view());
                    if (cr && cr->unsatisfied && cr->size && *cr->size == offset) {
                        // the part is the whole file already
                        if (auto m = io::rename(part, path); !m) {
                            co_return give_up(m.error(), true);
                        }
                        (void)io::remove(meta);
                        co_return r;
                    }
                    if (restarted) {
                        co_return give_up(status_error(url, st), false);
                    }
                    restarted = true;
                    offset = 0;
                    validator = string();
                    continue;
                } else if (!r->ok()) {
                    r->close();
                    co_return give_up(status_error(url, st), false);
                }
                if (!append) {
                    offset = 0;
                    validator = resume_validator(*r);
                }
                if (o.resume) {
                    if (validator.empty()) {
                        (void)io::remove(meta);
                    } else if (auto w = co_await io::async_write_file(meta, string::concat(url, "\n", validator, "\n")); !w) {
                        r->close();
                        co_return give_up(w.error(), false);
                    }
                }
                auto f = append ? co_await io::async_open(part, io::open_flags::write | io::open_flags::create | io::open_flags::append)
                                : co_await io::async_create(part);
                if (!f) {
                    r->close();
                    co_return give_up(f.error(), false);
                }
                auto written = co_await write_body(*r, *f);
                auto& done = written.result;
                auto closed = f->close();
                if (!done) {
                    const bool network = !written.file_failed && closed.has_value();
                    auto size = co_await io::async_stat(part);
                    if (network && retries > 0 && !validator.empty() && size) {
                        --retries;
                        offset = size->size;   // continued from the bytes that arrived
                        continue;
                    }
                    co_return give_up(done.error(), network);
                }
                if (!closed) {
                    co_return give_up(closed.error(), false);
                }
                if (auto m = io::rename(part, path); !m) {
                    co_return give_up(m.error(), false);
                }
                (void)io::remove(meta);
                co_return r;
            }
        }

    }

    SGCL_INLINE_HOT expected<response, io::error> client::download(const string& url, const string& path, const download_options& o) const {
        return async_download(url, path, o).wait();
    }

    SGCL_INLINE_HOT async::task<expected<response, io::error>> client::async_download(string url, string path, download_options o) const noexcept {
        return detail::download(*this, std::move(url), std::move(path), std::move(o));
    }

    // The file at url saved to path through a client of the process's (curl
    // -fo path url): the body streamed through path + ".part", a status
    // other than 2xx the error net::errc::http_status and no file; the
    // response, its body read. A body broken off is continued from where it
    // stopped (Range with If-Range) o.retries times, when the server named a
    // strong validator; o.resume continues a part an earlier call left. From
    // a thread; a task writes `co_await net::http::async_download(url, path)`
    SGCL_INLINE_HOT expected<response, io::error> download(const string& url, const string& path, const download_options& o = {}) {
        return detail::default_client_instance().download(url, path, o);
    }

    SGCL_INLINE_HOT async::task<expected<response, io::error>> async_download(string url, string path, download_options o = {}) noexcept {
        return detail::default_client_instance().async_download(std::move(url), std::move(path), std::move(o));
    }

    // --- response: the readers that need JSON and files ---

    SGCL_INLINE_HOT expected<encoding::json, io::error> response::json() const {
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
    SGCL_INLINE_HOT expected<T, io::error> response::json() const {
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

    SGCL_INLINE_HOT expected<uint64_t, io::error> response::save(const string& path) const {
        return async_save(path).wait();
    }

    SGCL_INLINE_HOT async::task<expected<uint64_t, io::error>> response::async_save(string path) const noexcept {
        return detail::save_body(*this, std::move(path));
    }
}

#include "detail/websocket_client.h"   // client::websocket and websocket::connect: the client of the process's above
