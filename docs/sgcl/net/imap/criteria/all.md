[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::all

```cpp
static criteria all() noexcept;
```

Returns the search key of every message: what the default constructor makes: IMAP's `ALL`.

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
    println("{}", net::imap::criteria::all().to_string());
}
```

Output:

```text
ALL
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
