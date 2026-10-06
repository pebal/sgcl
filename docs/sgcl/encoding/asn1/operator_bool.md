[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether this is an element: `false` for [asn1()](asn1.md), what [operator\[\]](operator_at.md) gives past the
end, so an OPTIONAL component at the end of a SEQUENCE is asked as `if (auto ext = tbs[7])`.

## Parameters

None.

## Return value

`true` for an element, `false` for none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 cert = encoding::asn1::sequence({
        encoding::asn1::explicit_tag(0, encoding::asn1::integer(2)),
        encoding::asn1::integer(4096),
        encoding::asn1::implicit_tag(1, encoding::asn1::bit_string(vector<byte>{byte(0x80)}, 1)),
        encoding::asn1::utf8_string("example.com")});

    for (size_t i : range(5)) {
        println("{}: {}", i, bool(cert[i]));
    }
}
```

Output:

```text
0: true
1: true
2: true
3: true
4: false
```

## See also

- [asn1()](asn1.md): no element
- [sgcl::encoding::asn1](README.md)
