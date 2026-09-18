# StopSource, StopToken

```cpp
#include "sgcl/Sgcl/Async/StopToken.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class StopSource;    // requests the stop; a child when made from a token
    class StopToken;     // sees it: a case of a Select, an awaitable, a flag
}
```

The same in the `sgcl` interface: [stop_source, stop_token](../../async/stop_token.md).

Cancellation, the way Go's `context` has it, under the names of `std::stop_source` and `std::stop_token`: a source requests the stop, the tokens handed down see it. What makes it fit the rest: a token is a [Channel](Channel.md) of signals closed when the stop is requested, so that a wait is cancelled the way anything else is waited for — a case of a [Select](Select.md) (`token.OnStop(f)`) beside the receive it bounds, or `co_await token.Stopped()` on its own. A deadline is a timer that requests the stop (`source.StopAfter(d)`, [Time](Time.md)). A source made from a token is a child: it stops when its parent stops and on its own, never the other way round, so that a request handler's source stops with the connection's, which stops with the server's. The parent knows its children through weak pointers: a child that is gone costs nothing, and a thousand children made and dropped leave nothing to walk.

The state is a managed object; a source or a token is one word, copied freely, alive while any copy is, or a timer holds it. Nothing is freed, nothing counted: a token kept in a task's frame, in a managed object or on a stack keeps the state, and a state nobody holds is garbage.

## Rules

- `RequestStop()` closes the token's channel: every wait on it, in a thread or a task, ends at once; the bodies of `OnStop` cases run in their selects; then the children are stopped, in no particular order. A second request is nothing.
- `IsStopRequested()` is the channel's `IsClosed()`: a load. A token made by default (`StopToken()`) has no source: `IsStopPossible()` is false and it never stops.
- A source lives where a `Ptr` may: on a stack or inside a managed object; a token the same. A token handed to a thread's closure goes through a [`RootPtr`](../Core/RootPtr.md) or by reference, as any tracked pointer does ([The rules](../../core/README.md#the-rules), 1).
- The stop is a state, not an event: a wait that starts after the stop ends at once, a child made after the stop is stopped at once. A parent keeps a weak entry per child made under it, and drops the entries of children gone as new ones register, so a long-lived source with a child per request holds no more than the live ones. `OnStop`, `Stopped()` and `Channel()` of an empty token are an error (an assertion in debug builds): `StopPossible()` says.

## Members

### StopToken

```cpp
StopToken() noexcept;                               // no source: never stops
bool IsStopRequested() const noexcept;
bool IsStopPossible() const noexcept;               // has a source
template<class F> auto OnStop(F f) const;           // a case of a Select: f() when stopped
auto Stopped() const noexcept;                      // an awaitable: co_await token.Stopped()
bool operator==(const StopToken&, const StopToken&) noexcept;   // the same source
InnerType& Inner() noexcept;
```

### StopSource

```cpp
StopSource();
explicit StopSource(const StopToken& parent);       // a child: stopped with the parent
StopToken Token() const noexcept;
bool IsStopRequested() const noexcept;
void RequestStop();
void StopAfter(Duration d);                         // the stop by a timer: a deadline, the children stopped too
InnerType& Inner() noexcept;
```

```cpp
StopSource server;
StopSource connection(server.Token());              // stops with the server
connection.StopAfter(30s);                          // or on its own, in 30 s
StopToken tok = connection.Token();
Channel<int> requests(8);
bool running = true;
while (running) {
    Select(
        requests.OnReceive([&](int r) { /* serve */ }),
        tok.OnStop([&] { running = false; })        // the server stopped, or the deadline
    );
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A server with workers: each worker serves requests until its token
// says stop; the server's source stops them all, a worker's own deadline
// stops it alone. The tokens live in the tasks' frames, on the managed
// heap, and the state they share is garbage once the last is gone.
Task<int> Worker(Channel<int>& requests, StopToken tok) {
    int served = 0;
    bool running = true;
    while (running) {
        co_await AsyncSelect(
            requests.OnReceive([&](int) { ++served; }),
            tok.OnStop([&] { running = false; })
        );
    }
    co_return served;
}

int main() {
    StopSource server;
    Channel<int> requests(8);
    StopSource shortLived(server.Token());
    shortLived.StopAfter(20ms);                                     // this worker's deadline
    Task a = Spawn(Worker(requests, server.Token()));
    Task b = Spawn(Worker(requests, shortLived.Token()));
    for (int i : Range(10)) {
        requests.Send(i);
    }
    ThisThread::SleepFor(50ms);                              // b's deadline passes
    server.RequestStop();                                           // a stops; b already did
    std::cout << a.Join() + b.Join() << " served\n";                // 10 served
    return a.Result() + b.Result() == 10 ? 0 : 1;
}
```

The output:

```
10 served
```

## See also

- [Select](Select.md): where `OnStop` is a case; [Time](Time.md): the deadline; [Channel](Channel.md): what a token is
- `tests/Sgcl/sgcl.cpp`: the behaviour above, checked.
