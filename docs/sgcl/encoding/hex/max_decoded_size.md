[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md)

# sgcl::encoding::hex::max_decoded_size

```cpp
static constexpr size_t max_decoded_size(size_t n) noexcept;
```

The most bytes `n` digits decode to, `n / 2`: Go's `hex.DecodedLen`, and the size the buffer of
[decode_to](decode_to.md) needs.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of digits |

## Return value

`n / 2`.

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
    println("{} {}", encoding::hex::max_decoded_size(64), encoding::hex::max_decoded_size(7));
}
```

Output:

```text
32 3
```

## See also

- [encoded_size](encoded_size.md): the other way
- [sgcl::encoding::hex](../hex.md)
