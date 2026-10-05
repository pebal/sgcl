[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::remove_header

```cpp
part& remove_header(const string& name);
```

Every field of the name taken out.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::part p("text/plain", "x");
    p.add_header("X-Tag", "a").remove_header("x-tag");
    println("{}", p.has_header("X-Tag"));
}
```

Output:

```text
false
```

## See also

- [set_header](set_header.md)
- [part](README.md)
