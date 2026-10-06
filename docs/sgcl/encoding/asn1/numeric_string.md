[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::numeric_string

```cpp
static asn1 numeric_string(const string& text);
```

A NumericString of the text: the digits and the space.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters |

## Return value

The element.

## Complexity

Linear in the length of the text.

## Exceptions

`invalid_argument` for any other character.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::numeric_string("0123 456");
    println("{} {}", encoding::hex::encode(e.bytes()), *e.as_string());
    try {
        encoding::asn1::numeric_string("12a");
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
12083031323320343536 0123 456
sgcl::encoding::asn1::numeric_string: a character the type does not allow
```

## See also

- [as_string](as_string.md): the text read
- [utf8_string](utf8_string.md)
- [sgcl::encoding::asn1](README.md)
