[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::smaller

```cpp
static criteria smaller(uint64_t octets) noexcept;
```

Returns the search key of the messages of fewer octets than `octets`: IMAP's `SMALLER`.

## Parameters

| Parameter | Description |
|---|---|
| `octets` | the size |

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
    println("{}", net::imap::criteria::smaller(1 << 20).to_string());
}
```

Output:

```text
SMALLER 1048576
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
