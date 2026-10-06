[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_big_integer

```cpp
optional<math::big_integer> as_big_integer() const noexcept;
```

The number of an INTEGER or an ENUMERATED of any size, or of an implicitly tagged primitive read as one: an
RSA modulus, a serial number of twenty bytes. `nullopt` for another type and for a content not in an INTEGER's
shortest form.

## Parameters

None.

## Return value

The number, or `nullopt`.

## Complexity

Linear in the size of the number.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer n = math::big_integer(1) << 160;
    encoding::asn1 serial = encoding::asn1::integer(-n);
    println(*serial.as_big_integer());
    println(serial.as_int());
}
```

Output:

```text
-1461501637330902918203684832716283019655932542976
nullopt
```

## See also

- [as_int](as_int.md): within `int64_t`
- [big_integer](../../math/big_integer/README.md)
- [sgcl::encoding::asn1](README.md)
