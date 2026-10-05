[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::on

```cpp
static criteria on(const time::date& d) noexcept;
```

Returns the search key of the messages received on the day: IMAP's `ON`. A day of the calendar, its time and zone
disregarded (RFC 9051): each message's in its own zone.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the day |

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
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    println("{}", net::imap::criteria::on(time::date(2026, 10, 1)).to_string());
}
```

Output:

```text
ON 1-Oct-2026
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
