[sgcl](../../README.md) › [encoding](../README.md) › [base64](README.md)

# sgcl::encoding::base64::encoded_size

```cpp
constexpr size_t encoded_size(size_t n) const noexcept;
```

The number of characters `n` bytes take: Go's `EncodedLen`. A padded codec writes four for every group of three
bytes, the last one included, `(n + 2) / 3 * 4`; a codec without padding the characters that carry the bits,
`(n * 8 + 5) / 6`. The size the buffer of [encode_to](encode_to.md) needs.

The arithmetic never wraps: an `n` whose characters no `size_t` can hold gives `SIZE_MAX`, the size no buffer has,
where a wrapped sum would be a small number and a write past the buffer.

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
    println("{} {}", encoding::base64::standard.encoded_size(4),
            encoding::base64::raw_url.encoded_size(4));
    println("{}", encoding::base64::standard.encoded_size(SIZE_MAX) == SIZE_MAX);
}
```

Output:

```text
8 6
true
```

## See also

- [max_decoded_size](max_decoded_size.md): the other way
- [encode_to](encode_to.md): into the caller's buffer
- [sgcl::encoding::base64](README.md)
