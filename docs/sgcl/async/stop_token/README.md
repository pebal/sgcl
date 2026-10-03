[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::stop_token

```cpp
#include "sgcl/async/stop_token.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class stop_token;
}
```

The side of cancellation a task sees: a [stop_source](../stop_source/README.md) requests the stop, the tokens handed down
from it see it. It is the standard's `std::stop_token` by name and Go's `context.Context` by use: a task takes a
token as a parameter, hands it to what it calls, and stops itself when it sees the stop; nothing stops a task from
outside.

What makes it fit the rest of the module: a token is a [channel](../channel/README.md) of signals closed when the stop is
requested, so a wait is cancelled the way anything else is waited for. `token.on_stop(f)` is a case of a
[select](../select.md) beside the receive it bounds; `co_await token.stopped()` waits for the stop alone;
`token.channel()` is the channel itself, from its receiving end (a [receive_channel](../receive_channel/README.md));
`token.stop_requested()` is a look, for a loop that computes. Where `std::stop_token` registers callbacks
(`std::stop_callback`), this one is waited on, so a task waiting for a stop holds no thread, and a stop ends every
wait on the token at once, in threads and tasks alike.

The state is a managed object, shared with the source and its other tokens; a token is one word, copied freely,
and keeps the state alive while any copy is. Nothing is freed and nothing counted: a state nobody holds is garbage.

## Rules

- A token lives where a `tracked_ptr` may ([The rules](../../core/README.md#the-rules), 1): on a stack, in a task's
  frame, inside a managed object, in the closure of an `sgcl::thread`; in a global or a std container, a
  `rooted<async::stop_token>`.
- The stop is a state, not an event: a wait that starts after the stop ends at once.
- A token made by default has no source: it never stops, `stop_possible()` is `false`, and `on_stop`, `stopped()`
  and `channel()` of it are an error (an assertion in debug builds). A function that takes a token it may be given
  empty looks at `stop_possible()` first.
- Every member may be called from any thread at any time.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](stop_token.md) | constructs a token with no source, which never stops |

#### Observers

| Function | Description |
|---|---|
| [stop_requested](stop_requested.md) | checks whether the stop has been requested |
| [stop_possible](stop_possible.md) | checks whether the token has a source |
| [channel](channel.md) | the receiving end of the channel the stop closes |

#### Waiting

| Function | Description |
|---|---|
| [stopped](stopped.md) | waits for the stop |
| [on_stop](on_stop.md) | the stop as a case of a select |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | checks whether two tokens belong to the same source |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A worker serves requests until its token says stop; the stop is one more
// case of its select, and the worker waits on both holding no thread
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

int main() {
    async::stop_source server;
    async::channel<int> requests;
    auto a = async::spawn(worker(requests, server.token()));
    auto b = async::spawn(worker(requests, server.token()));
    for (int i : range(10)) {
        requests.send(i).wait();
    }
    server.request_stop();
    println("{} served", a.wait() + b.wait());
}
```

Output:

```text
10 served
```

## See also

- [stop_source](../stop_source/README.md): the side that requests the stop, a deadline included
- [select](../select.md): where `on_stop` is a case
- [with_deadline](../with_deadline.md): a task raced against a token's stop
- [task_group](../task_group/README.md): a scope of tasks stopped as one
- [run](../run.md): the program's token, stopped by SIGINT or SIGTERM
