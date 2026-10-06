[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::to_json

```cpp
json to_json() const noexcept;
```

The value as [json](../json/README.md), by §6.1: an integer and a float a number (NaN and the infinities null),
a byte string base64url without padding (base64 under tag 22, hexadecimal under tag 23, the hints of §3.4.5.2), a
bignum the number of its digits, another tag its content, undefined and a simple value null, a map's key that is
not text its diagnostic notation.

## Parameters

None.

## Return value

The JSON value.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor c = encoding::cbor::map({{1, encoding::cbor::bytes(vector<byte>{byte(0xfb), byte(0xff)})},
                                            {"t", encoding::cbor::tagged(1, 1363896240)},
                                            {"x", encoding::cbor::undefined()}});
    println(c.to_json().to_string());
}
```

Output:

```text
{"1":"-_8","t":1363896240,"x":null}
```

## See also

- [from_json](from_json.md)
- [sgcl::encoding::cbor](README.md)
