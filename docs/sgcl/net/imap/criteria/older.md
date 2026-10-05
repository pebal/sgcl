[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::older

```cpp
static criteria older(duration d) noexcept;
```

Returns the search key of the messages received more than `d` ago (WITHIN, RFC 5032), to the second: IMAP's `OLDER`.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the time |

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
    println("{}", net::imap::criteria::older(std::chrono::hours(24)).to_string());
}
```

Output:

```text
OLDER 86400
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
