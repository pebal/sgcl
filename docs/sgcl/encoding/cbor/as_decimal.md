[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_decimal

```cpp
optional<pair<math::big_integer, int64_t>> as_decimal() const noexcept;
```

The mantissa and the exponent of ten of tag 4, a decimal fraction (§3.4.4): its content an array of an integer
exponent and an integer or bignum mantissa. `nullopt` for every other value.

## Parameters

None.

## Return value

The value, or `nullopt`.

## Complexity

Linear in the size of the mantissa.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    vector<byte> bytes = encoding::hex::decode("c48221196ab3").value();
    auto d = encoding::cbor::parse(bytes)->as_decimal();
    println("{} * 10^{}", d->first, d->second);
}
```

Output:

```text
27315 * 10^-2
```

## See also

- [decimal](decimal.md)
- [sgcl::encoding::cbor](README.md)
