[sgcl](../../README.md) › [async](../README.md) › [wait_group](../wait_group.md)

# sgcl::async::wait_group::wait_group

```cpp
/*(1)*/ wait_group() noexcept;
/*(2)*/ wait_group(const wait_group&) noexcept = default;
/*(3)*/ wait_group(wait_group&&) noexcept = default;
```

1. A new group at zero: its state made on the managed heap, with the channel of its first round closed from the
   start, so that a wait, or an [on_done](on_done.md) case, on a group that never counted up returns at once.
2. A handle of the same group: the copy shares the state, and an `add` or a `done` through either counts for both.
3. The same, taken from the other handle.

There is no empty group: every handle stands for one.

## Parameters

| Parameter | Description |
|---|---|
| `const wait_group&`, `wait_group&&` | the handle of the group to share |

## Complexity

- (1) Constant: the state and its first channel.
- (2–3) Constant: one tracked word copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::wait_group all;
    all.wait();  // at zero: returns at once
    async::wait_group same = all;
    same.add(2);
    println("{}", all.count());
}
```

Output:

```text
2
```

## See also

- [add](add.md), [done](done.md): count the work
- [operator=](operator_assign.md): makes the handle one of another group
- [sgcl::async::wait_group](../wait_group.md)
