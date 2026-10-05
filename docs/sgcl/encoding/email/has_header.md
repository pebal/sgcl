[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::has_header

```cpp
bool has_header(const string& name) const noexcept;
```

Whether the message has a field of the name, its case aside.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

`true` or `false`.

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
    auto m = encoding::email::parse("X-Empty:\r\n\r\n").value();
    println("{} {}", m.has_header("x-empty"), m.has_header("Subject"));
}
```

Output:

```text
true false
```

## See also

- [header](header.md)
- [email](README.md)
