[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::unseen

```cpp
static criteria unseen() noexcept;
```

Returns the search key of the messages without `\Seen`: IMAP's `UNSEEN`.

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
    println("{}", net::imap::criteria::unseen().to_string());
}
```

Output:

```text
UNSEEN
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
