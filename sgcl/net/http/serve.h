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
#include "../tls.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../crypto/read_secret.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../io/path.h"
#include "../../time/datetime.h"

#include <chrono>
#include <cstdint>
#include <string_view>
#include <utility>

// A directory served over HTTP in one call, and a handler over https with
// the certificate and the key read from their files: net::http::serve and
// net::http::serve_tls.
namespace sgcl::net::http {
    namespace detail {
        // The Content-Type of a file by its extension, as Go's built-in
        // table and mime.types have it; application/octet-stream for any
        // other (Go sniffs the first bytes; this does not)
        inline const char* content_type_of(std::string_view name) noexcept {
            const auto dot = name.rfind('.');
            const auto slash = name.rfind('/');
            if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
                return "application/octet-stream";
            }
            char ext[8] = {};
            const std::string_view e = name.substr(dot + 1);
            if (e.size() >= sizeof(ext)) {
                return "application/octet-stream";
            }
            for (size_t i = 0; i < e.size(); ++i) {
                const char c = e[i];
                ext[i] = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
            }
            static constexpr struct {
                const char* ext;
                const char* type;
            } table[] = {
                {"html", "text/html; charset=utf-8"},
                {"htm", "text/html; charset=utf-8"},
                {"css", "text/css; charset=utf-8"},
                {"js", "text/javascript; charset=utf-8"},
                {"mjs", "text/javascript; charset=utf-8"},
                {"json", "application/json"},
                {"txt", "text/plain; charset=utf-8"},
                {"md", "text/markdown; charset=utf-8"},
                {"csv", "text/csv; charset=utf-8"},
                {"xml", "text/xml; charset=utf-8"},
                {"svg", "image/svg+xml"},
                {"png", "image/png"},
                {"jpg", "image/jpeg"},
                {"jpeg", "image/jpeg"},
                {"gif", "image/gif"},
                {"webp", "image/webp"},
                {"avif", "image/avif"},
                {"heic", "image/heic"},
                {"ico", "image/vnd.microsoft.icon"},
                {"pdf", "application/pdf"},
                {"wasm", "application/wasm"},
                {"woff", "font/woff"},
                {"woff2", "font/woff2"},
                {"ttf", "font/ttf"},
                {"otf", "font/otf"},
                {"mp3", "audio/mpeg"},
                {"wav", "audio/wav"},
                {"ogg", "audio/ogg"},
                {"mp4", "video/mp4"},
                {"webm", "video/webm"},
                {"zip", "application/zip"},
                {"gz", "application/gzip"},
                {"tar", "application/x-tar"},
                {"7z", "application/x-7z-compressed"},
            };
            const std::string_view x(ext, e.size());
            for (const auto& t : table) {
                if (x == t.ext) {
                    return t.type;
                }
            }
            return "application/octet-stream";
        }

        // One request for a file under the directory: the name (the path
        // value, unescaped) through io::path::under, so that no name reaches
        // a file outside it ("..%2f" included); a directory by its
        // index.html, the URL of a directory without its slash redirected
        // (301) to it with one; a name that is not a file, or not under the
        // directory, 404. Content-Type by the extension, the file's length
        // and Last-Modified; If-Modified-Since not before the file's time
        // is 304 with no body. The file goes as the body (sendfile over
        // TCP)
        inline void serve_file(const string& directory, const request& req, response_writer w) {
            const string name = req.path_value("path");
            auto path = io::path::under(directory, name.empty() ? string(".") : name);
            if (!path) {
                w.error(status::not_found);
                return;
            }
            auto info = io::stat(*path);
            if (!info) {
                w.error(status::not_found);
                return;
            }
            if (info->is_directory()) {
                const string url_path = req.url().path();
                if (url_path.empty() || url_path.view().back() != '/') {
                    w.redirect(url_path + "/", status::moved_permanently);
                    return;
                }
                path = io::path::join(*path, "index.html");
                info = io::stat(*path);
                if (!info || info->is_directory()) {
                    w.error(status::not_found);
                    return;
                }
            }
            const int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(info->modified.time_since_epoch()).count();
            const auto modified = time::datetime::from_unix(seconds, time::zone::utc());
            w.headers().set_date("Last-Modified", modified);
            if (auto since = req.headers().date("If-Modified-Since"); since && since->unix() >= seconds) {
                w.set_status(status::not_modified);
                return;
            }
            auto file = io::open(*path);
            if (!file) {
                w.error(status::not_found);
                return;
            }
            w.set_header("Content-Type", content_type_of(path->view()));
            w.write(*file);
        }

        inline server file_server(const string& directory) {
            server srv;
            srv.route("GET /{path...}", [directory](request req, response_writer w) {
                serve_file(directory, req, w);
            });
            return srv;
        }

        // A certificate chain and its key from their files (PEM), the key
        // read into unmanaged memory
        inline expected<net::tls::config, io::error> tls_from_files(const string& certificate_file, const string& key_file) {
            auto chain = io::read_text(certificate_file);
            if (!chain) {
                return unexpected(chain.error());
            }
            auto key = crypto::read_secret(key_file);
            if (!key) {
                return unexpected(key.error());
            }
            auto id = net::tls::identity::from_pem(*chain, *key);
            if (!id) {
                return unexpected(id.error());
            }
            net::tls::config cfg;
            cfg.identities = {*id};
            return cfg;
        }
    }

    // The files of a directory over HTTP until the server stops, as Go's
    // http.ListenAndServe(address, http.FileServer(http.Dir(directory))):
    //
    //     net::http::serve(":8080", "public");
    //
    // GET and HEAD of /a/b.txt send directory/a/b.txt; a name that would
    // leave the directory (a "..", or "..%2f" before it is unescaped) is 404
    // and nothing is opened; a directory is its index.html (no listing), a
    // file that is not there 404. Content-Type by the extension,
    // Last-Modified and 304 for If-Modified-Since, the file sent by
    // sendfile. From a thread of the program; a task writes `co_await
    // net::http::async_serve(address, directory)`
    inline expected<void, io::error> serve(const string& address, const string& directory) {
        return detail::file_server(directory).serve(address);
    }

    inline async::task<expected<void, io::error>> async_serve(string address, string directory) {
        return detail::file_server(directory).async_serve(address);
    }

    // A handler for every path over https, the certificate chain and its
    // private key read from their PEM files (the key never in managed
    // memory), h2 and http/1.1 by ALPN, as Go's http.ListenAndServeTLS:
    //
    //     net::http::serve_tls(":8443", "cert.pem", "key.pem", [](net::http::request req, net::http::response_writer w) { ... });
    //
    // A file that cannot be read, or a key that is not the certificate's,
    // is the error, before anything listens
    template<class H>
    expected<void, io::error> serve_tls(const string& address, const string& certificate_file, const string& key_file, H handler) {
        auto cfg = detail::tls_from_files(certificate_file, key_file);
        if (!cfg) {
            return unexpected(cfg.error());
        }
        server srv;
        srv.route("/", std::move(handler));
        return srv.serve_tls(address, *cfg);
    }
}
