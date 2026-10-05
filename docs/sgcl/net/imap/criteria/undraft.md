[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::undraft

```cpp
static criteria undraft() noexcept;
```

Returns the search key of the messages without `\Draft`: IMAP's `UNDRAFT`.

## Parameters

None.

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
    println("{}", net::imap::criteria::undraft().to_string());
}
```

Output:

```text
UNDRAFT
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
