[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::properties, property_of, properties_of

```cpp
slice<const property> properties() const noexcept;                    // (1)
optional<property> property_of(const string& name) const noexcept;    // (2)
vector<property> properties_of(const string& name) const noexcept;    // (3)
```

1. The properties in order.
2. The first property of the name, in any case; `nullopt` when there is none.
3. Every property of the name, in order: EMAIL, TEL, ADR are often given several times.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the property's name |

## Return value

The properties, or the first.

## Complexity

(1) Constant; (2–3) linear in the properties.

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
    println(card.properties().size());
    println(card.property_of("N")->components());
    println(card.properties_of("email").size());
}
```

Output:

```text
7
["Kowalski", "Jan", "", "", ""]
2
```

## See also

- [text](text.md)
- [sgcl::encoding::vcard](README.md)
