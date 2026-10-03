[sgcl](../../README.md) › [async](../README.md) › [event](../event.md)

# sgcl::async::event::event

```cpp
event() noexcept;                          // (1)
event(const event&) noexcept = default;    // (2)
event(event&&) noexcept = default;         // (3)
```

1. A new event, not set: its state, an open channel of signals, made on the managed heap.
2. A handle of the same event: the copy shares the channel, and a set through either is a set of both.
3. The same, taken from the other handle.

There is no empty event: every handle stands for one.

## Parameters

| Parameter | Description |
|---|---|
| `const event&`, `event&&` | the handle of the event to share |

## Complexity

- (1) Constant: one allocation.
- (2–3) Constant: one tracked word copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::event done;
    async::event same = done;  // the same event
    async::event other;
    same.set();
    println("{} {}", done.is_set(), other.is_set());
}
```

Output:

```text
true false
```

## See also

- [operator=](operator_assign.md): makes the handle one of another event
- [operator==](operator_cmp.md): whether two handles are the same event
- [sgcl::async::event](../event.md)
