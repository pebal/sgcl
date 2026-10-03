[sgcl](../../README.md) › [async](../README.md) › [task_group](../task_group.md)

# sgcl::async::task_group::stop_requested

```cpp
bool stop_requested() const noexcept;
```

Checks whether the stop of the scope has been requested: by a child's exception, by
[request_stop](request_stop.md), or by the parent's token. Once `true`, it stays `true`.

## Parameters

None.

## Return value

`true` when the group's source is stopped, `false` otherwise.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> failing() {
    throw runtime_error("broken");
    co_return;
}

int main() {
    async::task_group g;
    println("{}", g.stop_requested());
    g.go(failing());
    try {
        g.wait();
    } catch (const runtime_error& e) {
        println("{}", e.what());
    }
    println("{}", g.stop_requested());  // the exception stopped the scope
}
```

Output:

```text
false
broken
true
```

## See also

- [request_stop](request_stop.md): the stop by hand
- [token](token.md): the same stop, as the children see it
- [sgcl::async::task_group](../task_group.md)
