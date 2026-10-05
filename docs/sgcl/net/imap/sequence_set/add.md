[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::add

```cpp
void add(uint32_t n) noexcept;                       // (1)
void add(uint32_t first, uint32_t last) noexcept;    // (2)
```

Adds a number (1) or a range (2) at the end of the set, `net::imap::last` for `*`.

## Parameters

| Parameter | Description |
|---|---|
| `n` | a number |
| `first`, `last` | the ends of a range |

## Return value

None.

## Complexity

Constant, amortized.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::sequence_set s;
    s.add(3);
    s.add(7, 9);
    s.add(20, net::imap::last);
    println("{}", s.to_string());
}
```

Output:

```text
3,7:9,20:*
```

## See also

- [sequence_set](README.md)
