[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::undeleted

```cpp
static criteria undeleted() noexcept;
```

Returns the search key of the messages without `\Deleted`: IMAP's `UNDELETED`.

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
    println("{}", net::imap::criteria::undeleted().to_string());
}
```

Output:

```text
UNDELETED
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
