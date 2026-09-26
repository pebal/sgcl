//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../io/detail/descriptor.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace sgcl::net::detail {
    using namespace sgcl::detail;
    // The descriptor of a socket: io's, which a file shares (io/detail/descriptor.h)
    using io::detail::WaitResult;
    using io::detail::Descriptor;
    using io::detail::Operation;

    // A new socket, as every socket of the module is: non-blocking,
    // close-on-exec, and on the systems that have it no SIGPIPE from a
    // write to a connection the peer closed (SO_NOSIGPIPE; on the others
    // the writes pass MSG_NOSIGNAL). macOS has neither SOCK_NONBLOCK nor
    // accept4, so the flags are set right after; io::command spawns with
    // POSIX_SPAWN_CLOEXEC_DEFAULT, so the window leaks nothing to a child
    // of this library.
    inline bool prepare_socket(int fd) noexcept {
        int fl = ::fcntl(fd, F_GETFL);
        if (fl < 0 || ::fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) {
            return false;
        }
        if (::fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
            return false;
        }
#ifdef SO_NOSIGPIPE
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        return true;
    }

#ifdef MSG_NOSIGNAL
    inline constexpr int SendFlags = MSG_NOSIGNAL;
#else
    inline constexpr int SendFlags = 0;
#endif
}
