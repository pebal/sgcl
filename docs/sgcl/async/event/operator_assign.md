[sgcl](../../README.md) › [async](../README.md) › [event](../event.md)

# sgcl::async::event::operator=

```cpp
event& operator=(const event&) noexcept = default;    // (1)
event& operator=(event&&) noexcept = default;         // (2)
```

Makes this handle one of the event the other stands for.

1. Copies the other handle: both stand for its event.
2. Takes the other handle.

The event this handle stood for is not touched: its waiters go on waiting for its set, and its channel is the
collector's once no handle or waiter holds it.

## Parameters

| Parameter | Description |
|---|---|
| `const event&`, `event&&` | the handle of the event to share |

## Return value

`*this`.

## Complexity

Constant: one tracked word stored.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::event first;
    async::event current;
    current = first;
    first.set();
    println("{}", current.is_set());
    current = async::event();  // a fresh one
    println("{}", current.is_set());
}
```

Output:

```text
true
false
```

## See also

- [(constructor)](event.md): a new event, or a handle of the same one
- [operator==](operator_cmp.md): whether two handles are the same event
- [sgcl::async::event](../event.md)
