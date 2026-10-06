[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::set

```cpp
vcard set(const property& p) const noexcept;
```

A new card with the property in place of every property of its name: where the first of them was, at the end when
there was none.

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
    auto card = encoding::vcard::parse(
        "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Jan Kowalski\r\nN:Kowalski;Jan;;;\r\n"
        "EMAIL;TYPE=work:jan@example.com\r\nEMAIL;TYPE=home:jan@home.example\r\n"
        "TEL;VALUE=uri;TYPE=cell:tel:+48-600-000-000\r\nORG:Example\\, Inc.\r\nEND:VCARD\r\n").value();
    print(card.set(encoding::content_line("EMAIL", "only@example.com")).erase("TEL").to_string());
}
```

Output:

```text
BEGIN:VCARD
VERSION:4.0
FN:Jan Kowalski
N:Kowalski;Jan;;;
EMAIL:only@example.com
ORG:Example\, Inc.
END:VCARD
```

## See also

- [add](add.md)
- [erase](erase.md)
- [sgcl::encoding::vcard](README.md)
