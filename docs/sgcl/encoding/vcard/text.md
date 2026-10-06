[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::text

```cpp
string text(const string& name, const string& fallback) const noexcept;
```

The first property of the name read as [TEXT](../content_line/text.md) (FN, ORG, NOTE, TITLE), or `fallback` when
there is none.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the property's name |
| `fallback` | what is given when there is none |

## Return value

The text.

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
    println("{} | {}", card.text("ORG", "?"), card.text("TITLE", "no title"));
}
```

Output:

```text
Example, Inc. | no title
```

## See also

- [properties](properties.md)
- [sgcl::encoding::vcard](README.md)
