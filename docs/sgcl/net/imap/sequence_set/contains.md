[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::contains

```cpp
bool contains(uint32_t n, uint32_t largest = UINT32_MAX) const noexcept;
```

Returns whether `n` is in the set, `*` standing for `largest`, the last number in use.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number |
| `largest` | the value of `*` |

## Return value

`true` when it is.

## Complexity

Linear in the number of ranges.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::sequence_set s("1:4,10:*");
    println("{} {} {}", s.contains(3), s.contains(7), s.contains(12, 20));
}
```

Output:

```text
true false true
```

## See also

- [expand](expand.md)
- [sequence_set](README.md)
