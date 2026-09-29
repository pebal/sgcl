//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Channels, select and wait_group on one thread, driven by a sequence of
// operations read from the input, against a model of queues and counters,
// with nothing that waits: try_send, try_receive, close, the counts, and
// select with an otherwise case, whose index must be a case that could be
// served at once (otherwise only when none could), with the model's effect
// of that case and its body called as the rules say (a receive of
// optional<T> with nothing for the close, a send's body only when it
// delivered). Waiting and the races between threads are the stress tests'
// and TSan's (tests/async); this holds the meaning of every operation on
// every state a channel can be in: a rendezvous, a buffer full or empty,
// closed with elements left or drained.
//
//   channel<int> a, b (capacities 0..4), channel<void> s (0..3), wait_group g
//
// The input: three capacities, then operations of two bytes (what, an
// argument).
//
// Built with libFuzzer (tests/fuzz/run.sh tests/async/fuzz/channels_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/async/channel.h"
#include "sgcl/async/select.h"
#include "sgcl/async/wait_group.h"

#include <cstdint>
#include <deque>
#include <optional>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // The model of a channel of int: its buffer, capacity and state
    struct Queue {
        std::deque<int> items;
        size_t capacity = 0;
        bool closed = false;

        bool can_send() const {
            return !closed && items.size() < capacity;
        }
    };

    void same(const async::channel<int>& c, const Queue& q) {
        check(c.size() == q.items.size() && c.empty() == q.items.empty() && c.closed() == q.closed && c.capacity() == q.capacity);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    Queue qa, qb;
    qa.capacity = data[0] % 5;
    qb.capacity = data[1] % 5;
    const size_t s_capacity = data[2] % 4;
    size_t s_count = 0;
    bool s_closed = false;
    long g_count = 0;

    async::channel<int> a(qa.capacity);
    async::channel<int> b(qb.capacity);
    async::channel<void> s(s_capacity);
    async::wait_group g;
    int next = 1;

    for (size_t i = 3; i + 1 < size; i += 2) {
        const uint8_t what = data[i], arg = data[i + 1];
        switch (what % 11) {
        case 0:
        case 2: {   // try_send
            auto& c = what % 11 == 0 ? a : b;
            auto& q = what % 11 == 0 ? qa : qb;
            const bool ok = c.try_send(next);
            check(ok == q.can_send());
            if (ok) {
                q.items.push_back(next);
            }
            ++next;
            break;
        }
        case 1:
        case 3: {   // try_receive
            auto& c = what % 11 == 1 ? a : b;
            auto& q = what % 11 == 1 ? qa : qb;
            auto got = c.try_receive();
            check(bool(got) == !q.items.empty());
            if (got) {
                check(*got == q.items.front());
                q.items.pop_front();
            }
            break;
        }
        case 4: {   // a signal sent
            const bool ok = s.try_send();
            check(ok == (!s_closed && s_count < s_capacity));
            s_count += ok;
            break;
        }
        case 5: {   // a signal received
            const bool ok = s.try_receive();
            check(ok == (s_count > 0));
            s_count -= ok;
            break;
        }
        case 6:     // close one of them (twice is nothing)
            if (arg % 3 == 0) {
                a.close();
                qa.closed = true;
            } else if (arg % 3 == 1) {
                b.close();
                qb.closed = true;
            } else {
                s.close();
                s_closed = true;
            }
            break;
        case 7:
            g.add(arg % 3 + 1);
            g_count += arg % 3 + 1;
            break;
        case 8:
            if (g_count > 0) {
                g.done();
                --g_count;
            }
            break;
        case 9: {   // select with otherwise: a case that can be served now, or otherwise
            const bool ready[4] = {
                !qa.items.empty() || qa.closed,     // a: receive
                qb.can_send() || qb.closed,         // b: send (a closed one serves it with nothing delivered)
                s_count > 0 || s_closed,            // s: a signal or the close
                g_count == 0,                       // g: at zero
            };
            const bool any = ready[0] || ready[1] || ready[2] || ready[3];
            int received_calls = 0, sent_calls = 0, signal_calls = 0, done_calls = 0, otherwise_calls = 0;
            std::optional<std::optional<int>> received;
            const int value = next++;
            const size_t index = async::select(
                a.on_receive([&](optional<int> v) {
                    ++received_calls;
                    received = v;
                }),
                b.on_send(value, [&] { ++sent_calls; }),
                s.on_receive([&] { ++signal_calls; }),
                g.on_done([&] { ++done_calls; }),
                async::otherwise([&] { ++otherwise_calls; })).wait();
            check(index <= 4);
            check(index == 4 ? !any : ready[index]);
            switch (index) {
            case 0:
                check(received_calls == 1 && received.has_value());
                if (!qa.items.empty()) {
                    check(*received == qa.items.front());
                    qa.items.pop_front();
                } else {
                    check(!*received);   // closed and drained
                }
                break;
            case 1:
                if (!qb.closed) {
                    check(sent_calls == 1);
                    qb.items.push_back(value);
                } else {
                    check(sent_calls == 0);   // nothing delivered to a closed channel
                }
                break;
            case 2:
                check(signal_calls == 1);
                if (s_count > 0) {
                    --s_count;
                }
                break;
            case 3:
                check(done_calls == 1);
                break;
            default:
                check(otherwise_calls == 1);
                break;
            }
            check(received_calls + sent_calls + signal_calls + done_calls + otherwise_calls <= 1);
            break;
        }
        default:
            same(a, qa);
            same(b, qb);
            check(s.size() == s_count && s.closed() == s_closed && g.count() == g_count);
            break;
        }
        same(a, qa);
        same(b, qb);
        check(g.count() == g_count);
    }
    // drained to the end: what is left comes out in order, then nothing
    a.close();
    while (auto v = a.try_receive()) {
        check(!qa.items.empty() && *v == qa.items.front());
        qa.items.pop_front();
    }
    check(qa.items.empty());
    return 0;
}
