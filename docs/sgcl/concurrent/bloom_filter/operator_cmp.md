[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::operator==, operator!= (sgcl::concurrent::bloom_filter)

```cpp
friend bool operator==(const bloom_filter& a, const bloom_filter& b) noexcept;
```

Checks whether two handles stand for the same filter: one a copy of the other, or both copies of one. Two filters
with the same bits made apart are not equal; [clone](clone.md) makes one. The `!=` is the one C++ writes from this
`==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same filter.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter a(10);
    concurrent::bloom_filter b = a;
    println("{} {}", a == b, a == a.clone());
}
```

Output:

```text
true false
```

## See also

- [clone](clone.md)
- [sgcl::concurrent::bloom_filter](README.md)
