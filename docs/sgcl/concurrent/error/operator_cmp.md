[sgcl](../../README.md) › [concurrent](../README.md) › [error](README.md)

# sgcl::concurrent::operator==, operator!= (sgcl::concurrent::error)

```cpp
friend bool operator==(const error& a, const error& b) noexcept;
```

Compares two errors: equal when they have the same sentence and the same byte. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors to compare |

## Return value

`true` when the sentence and the byte are the same.

## Complexity

Linear in the length of the sentences.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::error a("bad", 1), b("bad", 1), c("bad", 2);
    println("{} {}", a == b, a == c);
}
```

Output:

```text
true false
```

## See also

- [(constructor)](error.md)
- [sgcl::concurrent::error](README.md)
