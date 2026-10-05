[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::unkeyword

```cpp
static criteria unkeyword(const string& k) noexcept;
```

Returns the search key of the messages without the keyword `k`: IMAP's `UNKEYWORD`.

## Parameters

| Parameter | Description |
|---|---|
| `k` | the keyword |

## Return value

The criteria.

## Complexity

Linear in the size of the arguments.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{}", net::imap::criteria::unkeyword("$Work").to_string());
}
```

Output:

```text
UNKEYWORD $Work
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
