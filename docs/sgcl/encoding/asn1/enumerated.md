[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::enumerated

```cpp
template<class I> static asn1 enumerated(I value) noexcept;
```

An ENUMERATED, written as an INTEGER is (X.690 §8.4), under its own tag 10: LDAP's result codes, CRL
reasons. Of any integral type but `bool`: takes part only for those. [as_int](as_int.md) reads it back.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the number |

## Return value

The element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 reason = encoding::asn1::enumerated(4);   // superseded
    println("{} {}", encoding::hex::encode(reason.bytes()), *reason.as_int());
    println(reason.is(encoding::asn1::type::enumerated));
}
```

Output:

```text
0a0104 4
true
```

## See also

- [integer](integer.md): an INTEGER
- [as_int](as_int.md): the number read
- [sgcl::encoding::asn1](README.md)
