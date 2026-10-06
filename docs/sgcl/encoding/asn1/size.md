[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::size

```cpp
size_t size() const noexcept;
```

How many elements a constructed element holds, counted by stepping over them; 0 for a primitive one and for
[asn1()](asn1.md).

## Parameters

None.

## Return value

The count.

## Complexity

Linear in the elements inside.

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

    println("{} {} {}", cert.size(), cert[0].size(), cert[1].size());
}
```

Output:

```text
4 1 0
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::encoding::asn1](README.md)
