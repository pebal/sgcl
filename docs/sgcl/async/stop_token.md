# sgcl::stop_source, sgcl::stop_token

```cpp
#include "sgcl/async/stop_token.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class stop_source;   // requests the stop; a child when made from a token
    class stop_token;    // sees it: a case of a select, an awaitable, a flag
}
```

The same in the `Sgcl` interface: [StopSource, StopToken](../Sgcl/Async/StopToken.md).

Cancellation, the way Go's `context` has it, under the names of `std::stop_source` and `std::stop_token`: a source requests the stop, the tokens handed down see it. What makes it fit the rest: a token is a [channel](channel.md) of signals closed when the stop is requested, so that a wait is cancelled the way anything else is waited for — a case of a [select](select.md) (`token.on_stop(f)`) beside the receive it bounds, or `co_await token.stopped()` on its own. A deadline is a timer that requests the stop (`source.stop_after(d)`, [timer](timer.md)). A source made from a token is a child: it stops when its parent stops and on its own, never the other way round, so that a request handler's source stops with the connection's, which stops with the server's. The parent knows its children through weak pointers: a child that is gone costs nothing, and a thousand children made and dropped leave nothing to walk.

The state is a managed object; a source or a token is one word, copied freely, alive while any copy is, or a timer holds it. Nothing is freed, nothing counted: a token kept in a task's frame, in a managed object or on a stack keeps the state, and a state nobody holds is garbage.

## Rules

- `request_stop()` closes the token's channel: every wait on it, in a thread or a task, ends at once; the bodies of `on_stop` cases run in their selects; then the children are stopped, in no particular order. A second request is nothing.
- `stop_requested()` is the channel's `closed()`: a load. A token made by default (`stop_token()`) has no source: `stop_possible()` is false and it never stops.
- A source lives where a `tracked_ptr` may: on a stack or inside a managed object; a token the same. A token handed to a thread's closure goes through a [`root_ptr`](../core/root_ptr.md) or by reference, as any tracked pointer does ([The rules](../core/README.md#the-rules), 1).
- The stop is a state, not an event: a wait that starts after the stop ends at once, a child made after the stop is stopped at once. A parent keeps a weak entry per child made under it, and drops the entries of children gone as new ones register, so a long-lived source with a child per request holds no more than the live ones. `on_stop`, `stopped()` and `channel()` of an empty token are an error (an assertion in debug builds): `stop_possible()` says.

## Members

### stop_token

```cpp
stop_token() noexcept;                              // no source: never stops
bool stop_requested() const noexcept;
bool stop_possible() const noexcept;                // has a source
channel<void>& channel() const noexcept;            // the channel closed by the stop
template<class F> auto on_stop(F f) const;          // a case of a select: f() when stopped
auto stopped() const noexcept;                      // an awaitable: co_await token.stopped()
bool operator==(const stop_token&, const stop_token&) noexcept;   // the same source
```

### stop_source

```cpp
stop_source();
explicit stop_source(const stop_token& parent);     // a child: stopped with the parent
stop_token token() const noexcept;
bool stop_requested() const noexcept;
void request_stop();
void stop_after(duration d);                        // the stop by a timer: a deadline, the children stopped too
```

```cpp
sgcl::stop_source server;
sgcl::stop_source connection(server.token());       // stops with the server
connection.stop_after(30s);                         // or on its own, in 30 s
sgcl::stop_token tok = connection.token();
sgcl::channel<int> requests(8);
bool running = true;
while (running) {
    sgcl::select(
        requests.on_receive([&](int r) { /* serve */ }),
        tok.on_stop([&] { running = false; })       // the server stopped, or the deadline
    );
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A server with workers: each worker serves requests until its token
// says stop; the server's source stops them all, a worker's own deadline
// stops it alone. The tokens live in the tasks' frames, on the managed
// heap, and the state they share is garbage once the last is gone.
sgcl::task<int> worker(sgcl::channel<int>& requests, sgcl::stop_token tok) {
    int served = 0;
    bool running = true;
    while (running) {
        co_await sgcl::async_select(
            requests.on_receive([&](int) { ++served; }),
            tok.on_stop([&] { running = false; })
        );
    }
    co_return served;
}

int main() {
    sgcl::stop_source server;
    sgcl::channel<int> requests(8);
    sgcl::stop_source short_lived(server.token());
    short_lived.stop_after(20ms);                                   // this worker's deadline
    auto a = sgcl::spawn(worker(requests, server.token()));
    auto b = sgcl::spawn(worker(requests, short_lived.token()));
    for (int i : sgcl::range(10)) {
        requests.send(i);
    }
    sgcl::this_thread::sleep_for(50ms);                              // b's deadline passes
    server.request_stop();                                          // a stops; b already did
    std::cout << a.join() + b.join() << " served\n";                // 10 served
    return a.result() + b.result() == 10 ? 0 : 1;
}
```

The output:

```
10 served
```

## See also

- [select](select.md): where `on_stop` is a case; [timer](timer.md): the deadline; [channel](channel.md): what a token is
- `tests/async/stop_token.cpp`: every behaviour above, checked.
