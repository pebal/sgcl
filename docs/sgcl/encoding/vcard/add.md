[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::add

```cpp
vcard add(const property& p) const noexcept;
```

A new card with the property added after the others; the card itself never changes.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the property |

## Return value

The new card.

## Complexity

Linear in the properties.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    print(encoding::vcard().add(encoding::content_line::text("FN", "Ann")).add(encoding::content_line("EMAIL", "ann@example.com").with_group("item1")).to_string());
}
```

Output:

```text
BEGIN:VCARD
VERSION:4.0
FN:Ann
item1.EMAIL:ann@example.com
END:VCARD
```

## See also

- [set](set.md)
- [sgcl::encoding::vcard](README.md)
