[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::simple

```cpp
static cbor simple(uint8_t value);
```

A simple value of major type 7 (§3.3): 0 to 19 and 32 to 255, numbers a protocol gives a meaning of its own. 20 to
23 are false, true, null and undefined, which have their constructors, and 24 to 31 are no simple values.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the number |

## Return value

The value.

## Complexity

Constant.

## Exceptions

`invalid_argument` for 20 to 31.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::cbor::simple(16).to_string());
    println(encoding::hex::encode(encoding::cbor::simple(255).to_bytes()));
    try {
        encoding::cbor::simple(22);
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
simple(16)
f8ff
sgcl::encoding::cbor::simple: 20 to 31 are not simple values of their own
```

## See also

- [as_simple](as_simple.md)
- [sgcl::encoding::cbor](README.md)
