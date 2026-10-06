[sgcl](../../README.md) › [encoding](../README.md) › [msgpack](README.md)

# sgcl::encoding::msgpack::encode

```cpp
static vector<byte> encode(const cbor& value);
```

The bytes of a [cbor](../cbor/README.md) value in MessagePack, each in its shortest format: a positive or
negative fixint, the narrowest uint or int, a float32 when it holds the value exactly (else a float64), the fix
forms of str, array and map up to 31, 15 and 15, str8 and bin8 up to 255, then the 16 and 32-bit lengths, a fixext of
1, 2, 4, 8 or 16 bytes, else ext8, 16 or 32. A tag 1 (an instant) is written as the timestamp extension. Written
without recursion, however deep.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value |

## Return value

The bytes.

## Complexity

Linear in the size of the value.

## Exceptions

`invalid_argument` for what MessagePack has no format for: undefined, a simple value, an integer below -2^63, a tag other than 1 (a bignum among them).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (encoding::cbor v : {encoding::cbor(5), encoding::cbor(-33), encoding::cbor(300), encoding::cbor(1.5),
                             encoding::cbor(0.1), encoding::cbor("hi"), encoding::cbor::array({1, 2})}) {
        println("{} -> {}", v.to_string(), encoding::hex::encode(encoding::msgpack::encode(v)));
    }
    try {
        encoding::msgpack::encode(encoding::cbor::undefined());
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
5 -> 05
-33 -> d0df
300 -> cd012c
1.5 -> ca3fc00000
0.1 -> cb3fb999999999999a
"hi" -> a26869
[1, 2] -> 920102
sgcl::encoding::msgpack::encode: undefined or a simple value, which MessagePack has not
```

## See also

- [parse](parse.md)
- [sgcl::encoding::msgpack](README.md)
