[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::uid

```cpp
static criteria uid(const sequence_set& s) noexcept;
```

Returns the search key of the messages whose UIDs are in the set: IMAP's `UID`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the UIDs |

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
    println("{}", net::imap::criteria::uid(net::imap::sequence_set(100, net::imap::last)).to_string());
}
```

Output:

```text
UID 100:*
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
