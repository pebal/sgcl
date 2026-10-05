[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::to

```cpp
static criteria to(const string& s) noexcept;
```

Returns the search key of the messages whose To: holds `s`: IMAP's `TO`. The server matches a substring without regard
to case; strings with 8-bit text go with CHARSET UTF-8 to a server without UTF-8 of its own.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the text looked for, a substring, in any case |

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
    println("{}", net::imap::criteria::to("bob@example.com").to_string());
}
```

Output:

```text
TO "bob@example.com"
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
