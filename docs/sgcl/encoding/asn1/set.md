[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::set

```cpp
static asn1 set(std::initializer_list<asn1> elements) noexcept;    // (1)
static asn1 set(const vector<asn1>& elements) noexcept;            // (2)
```

A SET (or SET OF) of the elements in DER's order (X.690 §11.6): by their encodings compared as octet
strings, the shorter padded with zeros at its end — what Go's `Marshal` writes of a SET OF, and what a signature
over X.509's attributes and CMS's signed attributes is computed on. An [asn1()](asn1.md) among them is left out.

1. Of a list written out.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the elements inside, in any order |

## Return value

The element.

## Complexity

Linearithmic in the count of the elements, linear in their bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 s = encoding::asn1::set({encoding::asn1::integer(300), encoding::asn1::integer(2),
                                            encoding::asn1::integer(-1), encoding::asn1::boolean(true)});
    print(s.to_string());
}
```

Output:

```text
SET
  BOOLEAN true
  INTEGER 2
  INTEGER -1
  INTEGER 300
```

## See also

- [sequence](sequence.md): in the order given
- [sgcl::encoding::asn1](README.md)
