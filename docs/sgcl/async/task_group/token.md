[sgcl](../../README.md) › [async](../README.md) › [task_group](README.md)

# sgcl::async::task_group::token

```cpp
stop_token token() const noexcept;
```

Returns the token of the scope, the one to give the children: it is stopped by the first exception of a child, by
[request_stop](request_stop.md), by the parent's token, or by the group's end with children still running. A child
looks at it itself, with `stop_requested()`, `co_await tok.stopped()` or `tok.on_stop(f)` in a select, and leaves.

## Parameters

None.

## Return value

A [stop_token](../stop_token/README.md) of the group's source.

## Complexity

Constant: one tracked word copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> listener(async::stop_token tok) {
    co_await tok.stopped();  // the work: here, waiting for the stop
    println("stopped");
}

int main() {
    async::task_group g;
    g.go(listener(g.token()));
    g.request_stop();
    g.wait();
}
```

Output:

```text
stopped
```

## See also

- [request_stop](request_stop.md), [stop_requested](stop_requested.md): the stop of the scope
- [stop_token](../stop_token/README.md): what the token offers
- [sgcl::async::task_group](README.md)
