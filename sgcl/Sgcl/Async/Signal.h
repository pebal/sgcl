//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Signals({SIGINT, SIGTERM}): the signals of the process as a channel,
// the way Go's os/signal has them: the number of every signal delivered,
// for a task to await, a thread to receive, a Select to take as a case.
// ResetSignals gives the numbers their disposition back, IgnoreSignals
// sets them to be ignored. POSIX only for now.
#pragma once

#include "../../async/signal.h"
#include "../Core/Ptr.h"
#include "Channel.h"

#include <initializer_list>

namespace Sgcl {
    // A channel that gets the number of every signal of `numbers`
    // delivered to the process from now on; `capacity` elements held for
    // a receiver that is not there yet, the rest dropped (a burst coalesced)
    inline Ptr<Channel<int>> Signals(std::initializer_list<int> numbers, size_t capacity = 1) {
        Ptr<Channel<int>> ch = Make<Channel<int>>(capacity);
        sgcl::detail::signals_instance().notify(numbers, ch.Inner(), &ch->Inner());
        return ch;
    }

    // The disposition the numbers had before the first Signals back, the
    // channels registered for them forgotten; every number, for an empty list
    inline void ResetSignals(std::initializer_list<int> numbers = {}) {
        sgcl::reset_signals(numbers);
    }

    // The numbers ignored by the process, the channels registered for
    // them forgotten; ResetSignals undoes it
    inline void IgnoreSignals(std::initializer_list<int> numbers) {
        sgcl::ignore_signals(numbers);
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
