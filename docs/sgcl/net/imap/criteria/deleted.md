[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::deleted

```cpp
static criteria deleted() noexcept;
```

Returns the search key of the messages with `\Deleted`: IMAP's `DELETED`.

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
    println("{}", net::imap::criteria::deleted().to_string());
}
```

Output:

```text
DELETED
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
