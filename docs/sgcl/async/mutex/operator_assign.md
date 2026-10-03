[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md)

# sgcl::async::mutex::operator=

```cpp
/*(1)*/ mutex& operator=(const mutex&) noexcept = default;
/*(2)*/ mutex& operator=(mutex&&) noexcept = default;
```

Makes this handle one of the mutex the other stands for.

1. Copies the other handle: both stand for its mutex.
2. Takes the other handle.

The mutex this handle stood for is not touched: a lock held on it stays held, and its state is the collector's once
no handle holds it.

## Parameters

| Parameter | Description |
|---|---|
| `const mutex&`, `mutex&&` | the handle of the mutex to share |

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
    async::mutex a;
    async::mutex b;
    println("{}", a == b);
    b = a;
    println("{}", a == b);
    a.lock();
    println("{}", b.try_lock());
    b.unlock();
}
```

Output:

```text
false
true
false
```

## See also

- [(constructor)](mutex.md): a new mutex, or a handle of the same one
- [operator==](operator_cmp.md): whether two handles are the same mutex
- [sgcl::async::mutex](../mutex.md)
