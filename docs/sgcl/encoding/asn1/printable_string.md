[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::printable_string

```cpp
static asn1 printable_string(const string& text);
```

A PrintableString of the text: the letters A–Z and a–z, the digits, the space and `'()+,-./:=?`
(X.680 §41.4), what older certificates write their countries and names in. `*`, `@` and `&` are not among them,
though Go reads a PrintableString with `*` and `&`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters |

## Return value

The element.

## Complexity

Linear in the length of the text.

## Exceptions

`invalid_argument` for a character outside the set.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::printable_string("Example Ltd.");
    println("{} {}", encoding::hex::encode(e.bytes()), *e.as_string());
    try {
        encoding::asn1::printable_string("a@b");
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
130c4578616d706c65204c74642e Example Ltd.
sgcl::encoding::asn1::printable_string: a character the type does not allow
```

## See also

- [as_string](as_string.md): the text read
- [utf8_string](utf8_string.md)
- [sgcl::encoding::asn1](README.md)
