[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::ia5_string

```cpp
static asn1 ia5_string(const string& text);
```

An IA5String of the text: ASCII, what X.509 writes an e-mail address, a DNS name and a URI in.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters |

## Return value

The element.

## Complexity

Linear in the length of the text.

## Exceptions

`invalid_argument` for a byte past ASCII.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::ia5_string("user@example.com");
    println("{} {}", encoding::hex::encode(e.bytes()), *e.as_string());
    try {
        encoding::asn1::ia5_string("żółw");
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
161075736572406578616d706c652e636f6d user@example.com
sgcl::encoding::asn1::ia5_string: a character the type does not allow
```

## See also

- [as_string](as_string.md): the text read
- [utf8_string](utf8_string.md)
- [sgcl::encoding::asn1](README.md)
