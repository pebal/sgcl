[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::numbers

```cpp
static criteria numbers(const sequence_set& s) noexcept;
```

Returns the search key of the messages whose sequence numbers are in the set: IMAP's sequence set as a key.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the sequence numbers |

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
    println("{}", net::imap::criteria::numbers(net::imap::sequence_set(1, 10)).to_string());
}
```

Output:

```text
1:10
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
