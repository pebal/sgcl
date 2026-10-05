[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::set_header

```cpp
part& set_header(const string& name, const string& value);
```

The field `name` with the text `value`, in the place of the first field of the name, the others gone; at the end when there was none. Encoded when written, as [email::set_header](../email/set_header.md)'s.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |
| `value` | its text |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

`invalid_argument` when `name` is not a field name (RFC 5322 §3.6.8).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::part p("text/plain", "x");
    p.set_header("Content-Description", "zażółć");
    println("{}", p.header("content-description"));
}
```

Output:

```text
zażółć
```

## See also

- [header](header.md)
- [part](README.md)
