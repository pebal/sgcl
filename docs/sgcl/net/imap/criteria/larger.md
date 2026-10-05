[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::larger

```cpp
static criteria larger(uint64_t octets) noexcept;
```

Returns the search key of the messages of more octets than `octets` (RFC822.SIZE): IMAP's `LARGER`.

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
    println("{}", net::imap::criteria::larger(1 << 20).to_string());
}
```

Output:

```text
LARGER 1048576
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
