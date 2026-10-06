[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::as_bool

```cpp
optional<bool> as_bool() const noexcept;
```

The value of a BOOLEAN, or of an implicitly tagged primitive read as one: one byte, `00` or `FF` (any byte in
an element read as BER, true when not zero). `nullopt` for another type or another content.

## Parameters

None.

## Return value

The value, or `nullopt`.

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
    encoding::asn1 critical = encoding::asn1::boolean(true);
    println(critical.as_bool());
    println(encoding::asn1::integer(1).as_bool());
    vector<byte> ber{byte(1), byte(1), byte(1)};
    println(encoding::asn1::parse(ber).error().message());
    println(encoding::asn1::parse(ber, encoding::asn1::ber)->as_bool());
}
```

Output:

```text
true
nullopt
offset 0: a BOOLEAN not 00 or FF, which DER requires
true
```

## See also

- [boolean](boolean.md): one made
- [sgcl::encoding::asn1](README.md)
