[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::to_bytes

```cpp
vector<byte> to_bytes() const;                  // (1)
vector<byte> to_bytes(const style& s) const;    // (2)
```

The encoding of the value.

1. The preferred serialization (§4.1): definite lengths, every argument in its shortest form, a float in the
   shortest of half, single and double that holds it exactly, NaN as `f97e00`.
2. In the [style](../cbor-style.md) given: with `cbor::deterministic`, the keys of every map sorted by the bytes of
   their encodings (§4.2.1), so equal values write equal bytes whatever the order their maps were made in. The
   members move once a sorted map that holds them, and no key is written twice.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the style |

## Return value

The bytes.

## Complexity

Linear in the size of the value; (2) and linearithmic in the members of each map.

## Exceptions

`invalid_argument` for a value holding a MessagePack [extension](extension.md), which CBOR has no form for.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor m = encoding::cbor::map({{"b", 1}, {10, 2}, {"a", 3}, {1.5, 4}});
    println(encoding::hex::encode(m.to_bytes()));
    println(encoding::hex::encode(m.to_bytes(encoding::cbor::deterministic)));
}
```

Output:

```text
a46162010a02616103f93e0004
a40a02616103616201f93e0004
```

## See also

- [parse](parse.md)
- [cbor::style](../cbor-style.md)
- [sgcl::encoding::cbor](README.md)
