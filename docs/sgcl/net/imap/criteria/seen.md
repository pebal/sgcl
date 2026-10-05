[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::seen

```cpp
static criteria seen() noexcept;
```

Returns the search key of the messages with `\Seen`: IMAP's `SEEN`.

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
    println("{}", net::imap::criteria::seen().to_string());
}
```

Output:

```text
SEEN
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
