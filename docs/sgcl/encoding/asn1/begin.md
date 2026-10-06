[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::begin

```cpp
iterator begin() const noexcept;
```

An iterator to the first element inside a constructed element, equal to [end](end.md) when there is none: a
forward iterator whose `*` makes the element, a view, nothing copied. A range-for over an element walks its
elements.

## Parameters

None.

## Return value

The iterator.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <iterator>

using namespace sgcl;

int main() {
    encoding::asn1 cert = encoding::asn1::sequence({
        encoding::asn1::explicit_tag(0, encoding::asn1::integer(2)),
        encoding::asn1::integer(4096),
        encoding::asn1::implicit_tag(1, encoding::asn1::bit_string(vector<byte>{byte(0x80)}, 1)),
        encoding::asn1::utf8_string("example.com")});

    for (auto e : cert) {
        println("[{}] {}", e.tag(), e.constructed());
    }
    println(std::distance(cert.begin(), cert.end()));
}
```

Output:

```text
[0] true
[2] false
[1] false
[12] false
4
```

## See also

- [end](end.md)
- [operator\[\]](operator_at.md): an element by its index
- [sgcl::encoding::asn1](README.md)
