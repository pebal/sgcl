[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::bmp_string

```cpp
static asn1 bmp_string(const string& text);
```

A BMPString of the text: UCS-2, two bytes a character, big-endian — what PKCS #12 writes a friendly
name and a password in. The text is UTF-8; a code point past U+FFFF has no place in UCS-2.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters |

## Return value

The element.

## Complexity

Linear in the length of the text.

## Exceptions

`invalid_argument` when the text is not valid UTF-8 or holds a code point past U+FFFF.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::bmp_string("Zoë");
    println("{} {}", encoding::hex::encode(e.bytes()), *e.as_string());
    try {
        encoding::asn1::bmp_string("😀");
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
1e06005a006f00eb Zoë
sgcl::encoding::asn1::bmp_string: a code point past U+FFFF, which UCS-2 does not hold
```

## See also

- [as_string](as_string.md): the text read
- [utf8_string](utf8_string.md)
- [sgcl::encoding::asn1](README.md)
