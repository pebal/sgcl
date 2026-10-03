[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::operator==

```cpp
bool operator==(const query_params& other) const noexcept;
```

Compares two lists pair by pair, in their order: the same pairs in another order are another list. `!=` is made of it
by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the list compared with `*this` |

## Return value

`true` when the lists have the same pairs in the same order.

## Complexity

Linear in the number of pairs.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params a("x=1&y=2");
    println("{} {}", a == net::query_params("?x=1&y=2"), a == net::query_params("y=2&x=1"));
}
```

Output:

```text
true false
```

## See also

- [to_string](to_string.md): the text of the pairs
- [sgcl::net::query_params](../query_params.md)
