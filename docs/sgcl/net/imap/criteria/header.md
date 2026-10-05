[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::header

```cpp
static criteria header(const string& field, const string& value) noexcept;
```

Returns the search key of the messages with a field `field` holding `value`; an empty value: any message with the
field: IMAP's `HEADER`. The server matches a substring without regard to case; strings with 8-bit text go with CHARSET
UTF-8 to a server without UTF-8 of its own.

## Parameters

| Parameter | Description |
|---|---|
| `field` | the field's name, in any case |
| `value` | the text looked for |

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
    println("{}", net::imap::criteria::header("List-Id", "sgcl").to_string());
}
```

Output:

```text
HEADER "List-Id" "sgcl"
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
