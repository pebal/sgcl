[sgcl](../../README.md) › [async](../README.md) › [task_group](README.md)

# sgcl::async::task_group::count

```cpp
size_t count() const noexcept;
```

Returns the number of children started and not yet finished. Children finish on other threads at the same moment,
so the count is a look, not a promise; the wait for every child is [wait, operator co_await](wait.md).

## Parameters

None.

## Return value

The children not yet finished at the moment of the look.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> waiter(async::event go) {
    co_await go;
}

int main() {
    async::event go;
    async::task_group g;
    g.go(waiter(go));
    g.go(waiter(go));
    println("{}", g.count());
    go.set();
    g.wait();
    println("{}", g.count());
}
```

Output:

```text
2
0
```

## See also

- [wait, operator co_await](wait.md): waits for the count to reach zero
- [sgcl::async::task_group](README.md)
