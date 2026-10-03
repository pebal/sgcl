[sgcl](../../README.md) › [async](../README.md) › [task_group](README.md)

# sgcl::async::task_group::task_group

```cpp
explicit task_group(const stop_token& parent = stop_token());    // (1)
task_group(const task_group&) = delete;                          // (2)
```

1. A scope under `parent`: its own [stop_source](../stop_source/README.md) is made a child of the source the token belongs
   to, stopped with it, and at once when it is stopped already. An empty token, the default, makes a scope on its
   own, stopped only by its children's exceptions, by [request_stop](request_stop.md) and by its end.
2. A group is not copyable, and not movable: a scope has one place.

## Parameters

| Parameter | Description |
|---|---|
| `parent` | the token whose stop stops the scope; an empty token for a scope on its own |

## Complexity

Constant: the group's state on the managed heap, and the registration of its source with the parent's.

## Exceptions

`std::system_error` when the registration with the parent completes a stop of the parent's that wakes a waiting
task, the wake must start the scheduler's workers and a thread cannot be started
([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_source request;
    async::task_group alone;
    async::task_group under(request.token());
    request.request_stop();  // the parent's stop stops the scope under it
    println("{} {}", alone.stop_requested(), under.stop_requested());

    async::task_group late(request.token());  // under a stopped token: stopped at once
    println("{}", late.stop_requested());
}
```

Output:

```text
false true
true
```

## See also

- [token](token.md): the token the children are given
- [stop_source](../stop_source/README.md): a source made from a token
- [sgcl::async::task_group](README.md)
