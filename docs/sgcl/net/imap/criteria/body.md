[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::body

```cpp
static criteria body(const string& s) noexcept;
```

Returns the search key of the messages whose body (the text after the header, its text parts decoded) holds `s`:
IMAP's `BODY`. The server matches a substring without regard to case; strings with 8-bit text go with CHARSET UTF-8 to
a server without UTF-8 of its own.

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
    println("{}", net::imap::criteria::body("invoice").to_string());
}
```

Output:

```text
BODY "invoice"
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
