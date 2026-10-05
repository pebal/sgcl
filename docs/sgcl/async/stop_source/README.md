[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::stop_source

```cpp
#include "sgcl/async/stop_token.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class stop_source;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

The side of cancellation that requests the stop: the [stop_token](../stop_token/README.md)s handed down from it see it.
Cancellation the way Go's `context` has it, under the names of `std::stop_source` and `std::stop_token`.
`request_stop()` stops at once; a deadline is a timer that requests the stop, after a span
(`stop_after(d)`, Go's `context.WithTimeout`) or at a point (`stop_at(t)`, `WithDeadline`).

A source made from a token is a child, Go's derived context: it stops when its parent stops and on its own, never
the other way round, so that a request handler's source stops with the connection's, which stops with the
server's. The parent knows its children through weak pointers: a child that is gone costs nothing, and a thousand
children made and dropped leave nothing to walk.

The state is a managed object; a source is one word, copied freely, the copies sharing the state, alive while any
copy or token is, or an armed deadline holds it. Nothing is freed and nothing counted: a source kept in a task's
frame, in a managed object or on a stack keeps the state, and a state nobody holds is garbage.

## Rules

- `request_stop()` closes the token's channel: every wait on it, in a thread or a task, ends at once; the bodies
  of `on_stop` cases run in their selects; then the children are stopped, in no particular order. A second
  request is nothing.
- The stop is a state, not an event: a wait that starts after the stop ends at once, a child made after the stop
  is stopped at once.
- A parent keeps a weak entry per child made under it, and drops the entries of children gone as new ones
  register, so a long-lived source with a child per request holds no more than the live ones.
- A deadline is one timer: the earliest armed stands, a later one beside it arms nothing, and the stop, by hand
  or by the parent's, cancels it. The timer holds the deadline and not the state, so a source stopped by hand is
  not kept by a cancelled timer waiting in its heap.
- Every member may be called from any thread at any time.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](stop_source.md) | constructs a source, a child when given a token |

#### Observers

| Function | Description |
|---|---|
| [token](token.md) | a token of this source |
| [stop_requested](stop_requested.md) | checks whether the stop has been requested |

#### Modifiers

| Function | Description |
|---|---|
| [request_stop](request_stop.md) | requests the stop: every wait on the tokens ended, the children stopped |
| [stop_after](stop_after.md) | requests the stop after a span, by a timer |
| [stop_at](stop_at.md) | requests the stop at a point of the clock, by a timer |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> worker(async::channel<int> requests, async::stop_token token) {
    int served = 0;
    bool running = true;
    while (running) {
        co_await async::select(
            requests.on_receive([&](int) { ++served; }),
            token.on_stop([&] { running = false; })
        );
    }
    co_return served;
}

// The server's source stops every worker; a connection's source, a child,
// stops with it or on its own deadline, under a manual clock here
int main() {
    async::manual_clock clock;
    clock.install();
    async::stop_source server;
    async::stop_source connection(server.token());
    connection.stop_after(30s);
    async::channel<int> requests;
    auto a = async::spawn(worker(requests, server.token()));
    auto b = async::spawn(worker(requests, connection.token()));
    for (int i : range(10)) {
        requests.send(i).wait();
    }
    clock.advance(30s);
    println("{} {}", server.stop_requested(), connection.stop_requested());
    server.request_stop();
    println("{} served", a.wait() + b.wait());
}
```

Output:

```text
false true
10 served
```

## See also

- [stop_token](../stop_token/README.md): the side that sees the stop
- [with_timeout](../with_timeout.md): a source stopped when a task loses its race against the time
- [task_group](../task_group/README.md): a scope of tasks with a source of its own
- [manual_clock](../manual_clock/README.md): the deadlines of a test
