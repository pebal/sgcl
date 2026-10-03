[sgcl](../../README.md) › [async](../README.md) › [wait_group](../wait_group.md)

# sgcl::async::wait_group::operator=

```cpp
/*(1)*/ wait_group& operator=(const wait_group&) noexcept = default;
/*(2)*/ wait_group& operator=(wait_group&&) noexcept = default;
```

Makes this handle one of the group the other stands for.

1. Copies the other handle: both stand for its group.
2. Takes the other handle.

The group this handle stood for is not touched: its count stays, its waiters go on waiting, and its state is the
collector's once no handle or waiter holds it.

## Parameters

| Parameter | Description |
|---|---|
| `const wait_group&`, `wait_group&&` | the handle of the group to share |

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
    async::wait_group a;
    async::wait_group b;
    a.add(3);
    b = a;
    b.done();
    println("{} {}", a.count(), a == b);
}
```

Output:

```text
2 true
```

## See also

- [(constructor)](wait_group.md): a new group, or a handle of the same one
- [operator==](operator_cmp.md): whether two handles are the same group
- [sgcl::async::wait_group](../wait_group.md)
