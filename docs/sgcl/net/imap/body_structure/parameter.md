[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [body_structure](README.md)

# sgcl::net::imap::body_structure::parameter

```cpp
string parameter(const string& name) const noexcept;
```

Returns the value of a parameter of the part's Content-Type, its name in any case: `charset` of a text part,
`boundary` of a multipart, `name` of an attachment.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the parameter's name |

## Return value

The value, decoded; empty when the part has no such parameter.

## Complexity

Linear in the number of parameters.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::body_structure b;
    b.type = "text";
    b.subtype = "plain";
    b.parameters = {{"charset", "utf-8"}, {"format", "flowed"}};
    println("{}", b.parameter("CHARSET"));
    println("[{}]", b.parameter("delsp"));
}
```

Output:

```text
utf-8
[]
```

## See also

- [filename](filename.md)
- [body_structure](README.md)
