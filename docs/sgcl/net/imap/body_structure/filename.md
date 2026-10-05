[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [body_structure](README.md)

# sgcl::net::imap::body_structure::filename

```cpp
string filename() const noexcept;
```

Returns the file name of an attachment: Content-Disposition's `filename`, else Content-Type's `name`, the older place
mail programs put it.

## Parameters

None.

## Return value

The name, decoded (RFC 2231's continuations and charsets, RFC 2047's words); empty when the part has neither.

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
    net::imap::body_structure p;
    p.disposition = "attachment";
    p.disposition_parameters = {{"filename", "Zażółć.txt"}};
    p.parameters = {{"name", "old.txt"}};
    println("{}", p.filename());
    p.disposition_parameters = {};
    println("{}", p.filename());
}
```

Output:

```text
Zażółć.txt
old.txt
```

## See also

- [parameter](parameter.md)
- [body_structure](README.md)
