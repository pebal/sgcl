[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::visible_string

```cpp
static asn1 visible_string(const string& text);
```

A VisibleString of the text: printable ASCII, from the space to `~`, what Kerberos and some LDAP
attributes write.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters |

## Return value

The element.

## Complexity

Linear in the length of the text.

## Exceptions

`invalid_argument` for a control character or a byte past ASCII.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::visible_string("a b~");
    println("{} {}", encoding::hex::encode(e.bytes()), *e.as_string());
    try {
        encoding::asn1::visible_string("a\tb");
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
1a046120627e a b~
sgcl::encoding::asn1::visible_string: a character the type does not allow
```

## See also

- [as_string](as_string.md): the text read
- [utf8_string](utf8_string.md)
- [sgcl::encoding::asn1](README.md)
