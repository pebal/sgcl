[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_big_integer

```cpp
optional<math::big_integer> as_big_integer() const noexcept;
```

The value of an integer, or of a bignum (tag 2 or 3 over a byte string, §3.4.3), as a
[big_integer](../../math/big_integer/README.md) of any size; `nullopt` for every other kind and a tag 2 or 3 over
something else.

## Parameters

None.

## Return value

The value, or `nullopt`.

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
    vector<byte> bytes = encoding::hex::decode("c349010000000000000000").value();
    println(*encoding::cbor::parse(bytes)->as_big_integer());
    println(*encoding::cbor(-1).as_big_integer());
}
```

Output:

```text
-18446744073709551617
-1
```

## See also

- [as_int](as_int.md)
- [sgcl::encoding::cbor](README.md)
