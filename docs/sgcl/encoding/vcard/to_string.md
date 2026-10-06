[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::to_string

```cpp
string to_string() const;
```

The card as a file holds it, by the [rules](README.md#rules): `BEGIN:VCARD`, VERSION, the other properties in order,
`END:VCARD`.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the card.

## Exceptions

`invalid_argument` for a property's name or group that is none.

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
    print(card.set(encoding::content_line::text("NOTE", "met at the conference; follow up")).to_string());
}
```

Output:

```text
BEGIN:VCARD
VERSION:4.0
FN:Jan Kowalski
N:Kowalski;Jan;;;
EMAIL;TYPE=work:jan@example.com
EMAIL;TYPE=home:jan@home.example
TEL;VALUE=uri;TYPE=cell:tel:+48-600-000-000
ORG:Example\, Inc.
NOTE:met at the conference\; follow up
END:VCARD
```

## See also

- [parse](parse.md)
- [sgcl::encoding::vcard](README.md)
