[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::operator[]

```cpp
asn1 operator[](size_t index) const noexcept;
```

The element at the index inside a constructed element, a view of it; [asn1()](asn1.md) past the last one and
for a primitive element. The elements before it are stepped over, a header each: a loop over them all is
[begin](begin.md) and [end](end.md)'s.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the place, from 0 |

## Return value

The element, or `asn1()`.

## Complexity

Linear in the index.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 cert = encoding::asn1::sequence({
        encoding::asn1::explicit_tag(0, encoding::asn1::integer(2)),
        encoding::asn1::integer(4096),
        encoding::asn1::implicit_tag(1, encoding::asn1::bit_string(vector<byte>{byte(0x80)}, 1)),
        encoding::asn1::utf8_string("example.com")});

    println(*cert[1].as_int());
    println(*cert[0][0].as_int());
    println(bool(cert[9]));
}
```

Output:

```text
4096
2
false
```

## See also

- [size](size.md): how many
- [begin](begin.md): an iterator
- [sgcl::encoding::asn1](README.md)
