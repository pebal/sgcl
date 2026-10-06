[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_int

```cpp
optional<int64_t> as_int() const noexcept;
```

The number of an INTEGER or an ENUMERATED, or of an implicitly tagged primitive read as one, when an
`int64_t` holds it. `nullopt` for a number past it ([as_big_integer](as_big_integer.md) reads any), for another
type, and for a content not in an INTEGER's shortest form.

## Parameters

None.

## Return value

The number, or `nullopt`.

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
    println(encoding::asn1::integer(-129).as_int());
    println(encoding::asn1::enumerated(7).as_int());
    println(encoding::asn1::integer(uint64_t(-1)).as_int());
    println(encoding::asn1::implicit_tag(1, encoding::asn1::integer(300)).as_int());
    println(encoding::asn1::utf8_string("5").as_int());
}
```

Output:

```text
-129
7
nullopt
300
nullopt
```

## See also

- [as_big_integer](as_big_integer.md): any size
- [integer](integer.md): one made
- [sgcl::encoding::asn1](README.md)
