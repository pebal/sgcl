[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::map

```cpp
static cbor map(std::initializer_list<member> members) noexcept;    // (1)
static cbor map(const vector<member>& members) noexcept;            // (2)
```

A map of the members in their order, keys of any kind: COSE and CWT use small integers. A key given twice is
kept twice here; written and read back, it is a [parse](parse.md) error, so a program gives every key once.

1. Of a list written out, `{key, value}` each.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `members` | the keys and their values |

## Return value

The value.

## Complexity

Linear in the count of the members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor key = encoding::cbor::map({{1, 2}, {3, -7}, {-1, 1},
                                              {-2, encoding::cbor::bytes(vector<byte>(4, byte(0xaa)))}});
    println(key.to_string());
    println(*key[3].as_int());
}
```

Output:

```text
{1: 2, 3: -7, -1: 1, -2: h'aaaaaaaa'}
-7
```

## See also

- [array](array.md)
- [members](members.md)
- [operator\\[\\]](operator_at.md)
- [sgcl::encoding::cbor](README.md)
