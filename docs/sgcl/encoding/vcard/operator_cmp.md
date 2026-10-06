[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::operator== (sgcl::encoding::vcard)

```cpp
friend bool operator==(const vcard& a, const vcard& b) noexcept;
```

Whether the cards have the same properties in the same order (by [content_line](../content_line/operator_cmp.md)'s
equality); `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the cards |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the cards.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::vcard a;
    println("{} {}", a == encoding::vcard::parse(a.to_string()).value(), a == a.add(encoding::content_line("FN", "x")));
}
```

Output:

```text
true false
```

## See also

- [sgcl::encoding::vcard](README.md)
