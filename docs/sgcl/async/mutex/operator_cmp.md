[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md)

# sgcl::async::operator==, operator!= (sgcl::async::mutex)

```cpp
friend bool operator==(const mutex& a, const mutex& b) noexcept;
```

Checks whether two handles stand for the same mutex: `true` when they share the state, that is when one is a copy
of the other or both are copies of one. Two mutexes made apart are never equal, whatever their state. The `!=` is
the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same mutex, `false` otherwise.

## Complexity

Constant: two words compared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::mutex a;
    async::mutex b = a;
    async::mutex c;
    println("{} {} {}", a == b, a == c, a != c);
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](mutex.md): a handle of the same mutex
- [sgcl::async::mutex](../mutex.md)
