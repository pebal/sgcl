[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::version, formatted_name

```cpp
string version() const noexcept;           // (1)
string formatted_name() const noexcept;    // (2)
```

1. VERSION's value: `4.0`, `3.0`; empty without one.
2. FN's [text](../content_line/text.md): the name to show; empty without one.

## Parameters

None.

## Return value

The value.

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
    println("{} {}", card.version(), card.formatted_name());
}
```

Output:

```text
4.0 Jan Kowalski
```

## See also

- [text](text.md)
- [sgcl::encoding::vcard](README.md)
