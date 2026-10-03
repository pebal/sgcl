[sgcl](../../README.md) › [async](../README.md) › [wait_group](README.md)

# sgcl::async::wait_group::count

```cpp
long count() const noexcept;
```

Returns the count: the work added and not yet counted off. Other threads and tasks may count at the same moment, so
the count is a look, not a promise; the wait for zero is [wait, operator co_await](wait.md).

## Parameters

None.

## Return value

The count at the moment of the look.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::wait_group g;
    println("{}", g.count());
    g.add(3);
    g.done();
    println("{}", g.count());
}
```

Output:

```text
0
2
```

## See also

- [add](add.md), [done](done.md): change the count
- [sgcl::async::wait_group](README.md)
