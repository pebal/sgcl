[sgcl](../../README.md) › [async](../README.md) › [stop_source](README.md)

# sgcl::async::stop_source::stop_source

```cpp
stop_source() noexcept;                            // (1)
explicit stop_source(const stop_token& parent);    // (2)
```

1. A source of its own: a new state on the managed heap, not stopped.
2. A child of the source `parent` belongs to: a new state that stops when the parent's stops, and on its own,
   never the other way round. A parent stopped already stops the child at once. An empty `parent` (a token made by
   default) has nothing to stop the child, which is then a source of its own.

A child registers in its parent's list as a weak entry, so the parent does not keep it alive; before it
registers, it drops the entries of children gone from the head of the list, so that a long-lived parent with a
child per request holds no more than the live ones. A parent's stop that passes the list while a child registers
is seen by the child: one of the two always sees the other.

Copies of a source share its state: the copy and the assignment are the implicit ones, noexcept, one word each.

## Parameters

| Parameter | Description |
|---|---|
| `parent` | a token of the source to stop with |

## Complexity

- (1) Constant: one allocation.
- (2) Constant: one allocation, plus the entries of children gone that the registration drops.

## Exceptions

- (1) None.
- (2) `std::system_error` when a stop it makes (of itself, under a parent stopped already, or of a sibling the
  parent's stop passed) wakes a task and the wake starts the scheduler's workers, which cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_source server;
    async::stop_source connection(server.token());
    async::stop_source request(connection.token());
    auto show = [&] {
        println("{} {} {}", server.stop_requested(), connection.stop_requested(),
                request.stop_requested());
    };

    request.request_stop();  // the child alone
    show();
    server.request_stop();  // the parent, and everything under it
    show();

    async::stop_source late(server.token());  // made after the stop
    println("{}", late.stop_requested());
}
```

Output:

```text
false false true
true true true
true
```

## See also

- [token](token.md): the token a child is made from
- [request_stop](request_stop.md): the stop
- [sgcl::async::stop_source](README.md)
