[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::has_header

```cpp
bool has_header(const string& name) const noexcept;
```

Whether the part has a field of the name.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

`true` or `false`.

## Complexity

Linear in the size of the head.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::email::part p("text/plain", "x");
    println("{} {}", p.has_header("Content-Type"), p.has_header("Content-ID"));
}
```

Output:

```text
true false
```

## See also

- [set_header](set_header.md)
- [part](README.md)
