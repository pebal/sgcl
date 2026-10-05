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
#include "detail/mime.h"
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
            if (!info->is_regular()) {
                // a FIFO, a socket, a device: not a file to send (an open
                // of a FIFO waits for a writer, the worker with it)
                w.error(status::not_found);
                return;
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

        SGCL_INLINE_HOT server file_server(const string& directory) noexcept {
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
    // file that is not there, or is not a regular file (a FIFO), 404. Content-Type by the extension,
    // Last-Modified and 304 for If-Modified-Since, the file sent by
    // sendfile. From a thread of the program; a task writes `co_await
    // net::http::async_serve(address, directory)`
    SGCL_INLINE_HOT expected<void, io::error> serve(const string& address, const string& directory) {
        return detail::file_server(directory).serve(address);
    }

    SGCL_INLINE_HOT async::task<expected<void, io::error>> async_serve(string address, string directory) noexcept {
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
    SGCL_INLINE_HOT expected<void, io::error> serve_tls(const string& address, const string& certificate_file, const string& key_file, H handler) {
        auto cfg = detail::tls_from_files(certificate_file, key_file);
        if (!cfg) {
            return unexpected(cfg.error());
        }
        server srv;
        srv.route("/", std::move(handler));
        return srv.serve_tls(address, *cfg);
    }

    // The same with a TLS config of the program's: an identity of its own,
    // or identity_for (acme::manager's tls_config(), certificates obtained
    // and renewed by themselves):
    //
    //     net::http::serve_tls(":443", manager.tls_config(), handler);
    //
    // The config's ALPN list completed as server::serve_tls completes it
    template<class H>
    SGCL_INLINE_HOT expected<void, io::error> serve_tls(const string& address, const net::tls::config& c, H handler) {
        server srv;
        srv.route("/", std::move(handler));
        return srv.serve_tls(address, c);
    }
}
