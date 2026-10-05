[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::header_all

```cpp
vector<string> header_all(const string& name) const;
```

Every value of the field `name`, in their order.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

The values.

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
    p.add_header("X-Tag", "a").add_header("X-Tag", "b");
    println("{}", p.header_all("x-tag").size());
}
```

Output:

```text
2
```

## See also

- [set_header](set_header.md)
- [part](README.md)
