[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::draft

```cpp
static criteria draft() noexcept;
```

Returns the search key of the messages with `\Draft`: IMAP's `DRAFT`.

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
    println("{}", net::imap::criteria::draft().to_string());
}
```

Output:

```text
DRAFT
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
