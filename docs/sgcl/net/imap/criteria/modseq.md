[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::modseq

```cpp
static criteria modseq(uint64_t n) noexcept;
```

Returns the search key of the messages whose mod-sequence is `n` or past it (CONDSTORE, RFC 7162): IMAP's `MODSEQ`.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the mod-sequence |

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
    println("{}", net::imap::criteria::modseq(1000).to_string());
}
```

Output:

```text
MODSEQ 1000
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
