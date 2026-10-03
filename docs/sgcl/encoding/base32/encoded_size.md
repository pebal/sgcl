[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md)

# sgcl::encoding::base32::encoded_size

```cpp
constexpr size_t encoded_size(size_t n) const noexcept;
```

The number of characters `n` bytes take: Go's `EncodedLen`. A padded codec writes eight for every group of five
bytes, the last one included, `(n + 4) / 5 * 8`; a codec without padding the characters that carry the bits,
`(n * 8 + 4) / 5`. The size the buffer of [encode_to](encode_to.md) needs.

The arithmetic never wraps: an `n` whose characters no `size_t` can hold gives `SIZE_MAX`, the size no buffer
has.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes |

## Return value

The number of characters, or `SIZE_MAX` when it does not fit a `size_t`.

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
    auto raw = encoding::base32::standard.without_padding();
    println("{} {}", encoding::base32::standard.encoded_size(6), raw.encoded_size(6));
    println("{}", raw.encoded_size(SIZE_MAX) == SIZE_MAX);
}
```

Output:

```text
16 10
true
```

## See also

- [max_decoded_size](max_decoded_size.md): the other way
- [encode_to](encode_to.md): into the caller's buffer
- [sgcl::encoding::base32](../base32.md)
