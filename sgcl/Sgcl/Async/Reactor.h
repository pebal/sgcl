//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The readiness of file descriptors, as channels: Readable(fd) gets one
// signal when fd can be read without blocking, Writable(fd) when it can
// be written; a task awaits them and holds no thread meanwhile. One
// thread on the kernel's queue under them (kqueue; epoll and IOCP to
// come): the foundation of the io and net modules.
#pragma once

#include "../../async/reactor.h"
#include "../Core/Ptr.h"
#include "Channel.h"

namespace Sgcl {
    // A channel signalled once when fd can be read without blocking, then closed
    inline Ptr<Channel<void>> Readable(int fd) {
        Ptr<Channel<void>> ch = Make<Channel<void>>(1);
        sgcl::detail::reactor_instance().watch(fd, false, ch.Inner(), &ch->Inner());
        return ch;
    }

    // The same for a write
    inline Ptr<Channel<void>> Writable(int fd) {
        Ptr<Channel<void>> ch = Make<Channel<void>>(1);
        sgcl::detail::reactor_instance().watch(fd, true, ch.Inner(), &ch->Inner());
        return ch;
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

