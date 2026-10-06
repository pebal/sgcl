[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::utf8_string

```cpp
static asn1 utf8_string(const string& text);
```

A UTF8String of the text: what X.509's names and most new schemas write.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters |

## Return value

The element.

## Complexity

Linear in the length of the text.

## Exceptions

`invalid_argument` when the text is not valid UTF-8.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::utf8_string("zażółć");
    println("{} {}", encoding::hex::encode(e.bytes()), *e.as_string());
    try {
        encoding::asn1::utf8_string(string("\xff"));
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
0c0a7a61c5bcc3b3c582c487 zażółć
sgcl::encoding::asn1::utf8_string: invalid UTF-8
```

## See also

- [as_string](as_string.md): the text read
- [printable_string](printable_string.md)
- [sgcl::encoding::asn1](README.md)
