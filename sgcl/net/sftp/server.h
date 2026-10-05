//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "detail/fs.h"
#include "detail/protocol.h"
#include "detail/server_core.h"
#include "../ssh/server.h"
#include "../../async/blocking.h"
#include "../../io/fs.h"

#include <algorithm>
#include <memory>
#include <string>
#include <sys/stat.h>

// An SFTP server (version 3, OpenSSH's extensions) over a session of
// ssh::server: a directory served as the client's "/", every path the client
// names resolved within it (detail/fs.h: no "..", absolute path or symlink
// leads out), read-only when asked.
//
//   srv.handle([](net::ssh::server_session s) {
//       if (s.subsystem() == "sftp") {
//           (void)net::sftp::serve(s, "/srv/files");
//       }
//   });
namespace sgcl::net::sftp {
    namespace detail {
        inline io::error serve_error(error_code code, const string& root, std::string_view what) noexcept {
            return io::error(code, "sftp serve", string(std::string(root.view()) + ": " + std::string(what)));
        }

        // The requests of a session served in turn, on the calling thread
        // (a thread of the blocking pool: the blocking forms of the
        // session's streams), until the client's end of its input
        inline expected<void, io::error> serve_session(const ssh::server_session& s, std::unique_ptr<Fs> fs, const server_options& o, const string& root) {
            ServerCore core(std::move(fs), o);
            io::reader in = s.input();
            io::writer out = s.output();
            // the input read in chunks, every whole request in a chunk served
            // before the next read, their answers written as one: a client
            // keeping many requests in flight costs one read and one write
            // per chunk, not per request
            Bytes input(size_t(1) << 18), reply;
            size_t head = 0, end = 0;
            for (;;) {
                bool more = false;
                while (end - head >= 4) {
                    const uint32_t n = load32(input.data() + head);
                    if (n == 0 || n > MaxPacket) {
                        return unexpected(serve_error(net::make_error_code(net::errc::sftp_protocol), root, "a packet of a length out of range"));
                    }
                    if (end - head - 4 < n) {
                        break;
                    }
                    if (!core.handle(input.data() + head + 4, n, reply)) {
                        return unexpected(serve_error(net::make_error_code(net::errc::sftp_protocol), root, "a packet that breaks the protocol (or no INIT first)"));
                    }
                    head += 4 + size_t(n);
                    if (reply.size() >= MaxPacket) {
                        more = true;   // the answers so far go out before more requests are served
                        break;
                    }
                }
                if (!reply.empty()) {
                    auto w = out.write(ssh::detail::bytes_of(reply));
                    if (!w) {
                        return unexpected(w.error());
                    }
                    reply.clear();
                    if (reply.capacity() > (size_t(1) << 20)) {
                        Bytes().swap(reply);
                    }
                }
                if (more) {
                    continue;
                }
                if (head > 0) {
                    sgcl::detail::move_bytes(input.data(), input.data() + head, end - head);
                    end -= head;
                    head = 0;
                }
                const size_t want = end >= 4 ? 4 + size_t(load32(input.data())) : 0;
                if (input.size() < std::max(want, end + 4096)) {
                    input.resize(std::max(want, input.size() * 2));
                } else if (end == 0 && input.size() > (size_t(1) << 19)) {
                    Bytes(size_t(1) << 18).swap(input);   // back to its usual size after a large request
                }
                auto r = in.read(ssh::detail::mutable_bytes_of(input.data() + end, input.size() - end));
                if (!r) {
                    return unexpected(r.error());
                }
                if (*r == 0) {
                    if (end == 0) {
                        return {};   // the client's end
                    }
                    return unexpected(serve_error(make_error_code(io::errc::unexpected_eof), root, "a packet cut short"));
                }
                end += *r;
            }
        }
    }

    // The directory `root` served over the session as SFTP version 3, until
    // the client ends it: the client's "/" is the root, a relative path is
    // under options::home; every path is resolved within the root (a ".."
    // stops there, a symlink is followed within it, an absolute target from
    // the root), so that no request reaches a file outside it. Read-only,
    // every change is refused. The requests are served in turn, each file
    // operation the system's (pread, pwrite, rename …) on a thread of the
    // blocking pool, so a handler returning void calls serve, one returning
    // async::task<> awaits async_serve
    // `serve(...)` on this thread, `co_await async_serve(...)` in a task
    inline expected<void, io::error> serve(const ssh::server_session& s, const string& root, const server_options& o = {}) {
        struct ::stat st;
        if (::stat(root.c_str(), &st) != 0) {
            return unexpected(detail::serve_error(error_code(errno, std::system_category()), root, "the root"));
        }
        if (!S_ISDIR(st.st_mode)) {
            return unexpected(detail::serve_error(error_code(ENOTDIR, std::system_category()), root, "the root"));
        }
        return detail::serve_session(s, std::make_unique<detail::DiskFs>(std::string(root.view())), o, root);
    }

    inline async::task<expected<void, io::error>> async_serve(ssh::server_session s, string root, server_options o = {}) noexcept {
        co_return co_await async::spawn_blocking([s, root, o] { return serve(s, root, o); });
    }
}
