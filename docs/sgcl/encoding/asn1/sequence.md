[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::sequence

```cpp
static asn1 sequence(std::initializer_list<asn1> elements) noexcept;    // (1)
static asn1 sequence(const vector<asn1>& elements) noexcept;            // (2)
```

A SEQUENCE (or SEQUENCE OF) of the elements, in their order. An [asn1()](asn1.md) among them is left
out, so an OPTIONAL component is a condition in the list. Each element's bytes are copied once into the new one.

1. Of a list written out.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the elements inside |

## Return value

The element.

## Complexity

Linear in the bytes of the elements.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 name = encoding::asn1::sequence({
        encoding::asn1::object_identifier(encoding::asn1::oid("2.5.4.3")), encoding::asn1::utf8_string("example.com")});
    println(encoding::hex::encode(name.bytes()));

    vector<encoding::asn1> numbers;
    for (int i : range(3)) {
        numbers.push_back(encoding::asn1::integer(i));
    }
    print(encoding::asn1::sequence(numbers).to_string());
}
```

Output:

```text
301206035504030c0b6578616d706c652e636f6d
SEQUENCE
  INTEGER 0
  INTEGER 1
  INTEGER 2
```

## See also

- [set](set.md): in DER's order
- [operator\[\]](operator_at.md): an element inside
- [sgcl::encoding::asn1](README.md)
