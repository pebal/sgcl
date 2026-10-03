[sgcl](../../README.md) › [encoding](../README.md) › [hex](README.md)

# sgcl::encoding::hex::encoded_size

```cpp
static constexpr size_t encoded_size(size_t n) noexcept;
```

The number of digits `n` bytes take, `2 * n`: Go's `hex.EncodedLen`, and the size the buffer of
[encode_to](encode_to.md) needs. The arithmetic never wraps: an `n` whose digits no `size_t` can hold gives
`SIZE_MAX`, the size no buffer has.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes |

## Return value

The number of digits, or `SIZE_MAX` when it does not fit a `size_t`.

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
    println("{}", encoding::hex::encoded_size(32));
    println("{}", encoding::hex::encoded_size(SIZE_MAX) == SIZE_MAX);
}
```

Output:

```text
64
true
```

## See also

- [max_decoded_size](max_decoded_size.md): the other way
- [sgcl::encoding::hex](README.md)
