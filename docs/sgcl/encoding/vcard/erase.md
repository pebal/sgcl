[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::erase

```cpp
vcard erase(const string& name) const noexcept;
```

A new card without the properties of the name, in any case; the card as it is when there are none.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the properties' name |

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
    println("{} {}", card.erase("email").properties().size(), card.properties().size());
}
```

Output:

```text
5 7
```

## See also

- [set](set.md)
- [sgcl::encoding::vcard](README.md)
