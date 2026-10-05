[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::flagged

```cpp
static criteria flagged() noexcept;
```

Returns the search key of the messages with `\Flagged`: IMAP's `FLAGGED`.

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
    println("{}", net::imap::criteria::flagged().to_string());
}
```

Output:

```text
FLAGGED
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
