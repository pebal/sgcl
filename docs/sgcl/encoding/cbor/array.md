[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::array

```cpp
static cbor array(std::initializer_list<cbor> elements) noexcept;    // (1)
static cbor array(const vector<cbor>& elements) noexcept;            // (2)
```

An array of the elements in their order.

1. Of a list written out.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the elements |

## Return value

The value.

## Complexity

Linear in the count of the elements.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::cbor::array({1, "two", encoding::cbor::array({3.5})}).to_string());
    vector<encoding::cbor> squares;
    for (int i : range(1, 5)) {
        squares.push_back(i * i);
    }
    println(encoding::hex::encode(encoding::cbor::array(squares).to_bytes()));
}
```

Output:

```text
[1, "two", [3.5]]
8401040910
```

## See also

- [map](map.md)
- [elements](elements.md)
- [sgcl::encoding::cbor](README.md)
