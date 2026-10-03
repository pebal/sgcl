[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md)

# sgcl::encoding::base64::max_decoded_size

```cpp
constexpr size_t max_decoded_size(size_t n) const noexcept;
```

The most bytes a text of `n` characters decodes to: Go's `DecodedLen`, and the size the buffer of
[decode_to](decode_to.md) needs. A padded codec counts every group the text starts as whole,
`(n + 3) / 4 * 3`, so that a buffer of that size holds whatever a text of `n` characters writes before it is
found wrong; without padding it is the bits of `n` characters, `n * 6 / 8`. The padding and, in a lenient
decoding, the line endings make the bytes fewer.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of characters |

## Return value

The most bytes the text decodes to.

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
    println("{} {}", encoding::base64::standard.max_decoded_size(6),
            encoding::base64::raw_standard.max_decoded_size(6));
    println("{}", encoding::base64::standard.decode("aGVsbG8=")->size());
}
```

Output:

```text
6 4
5
```

## See also

- [encoded_size](encoded_size.md): the other way
- [decode_to](decode_to.md): into the caller's buffer
- [sgcl::encoding::base64](../base64.md)
