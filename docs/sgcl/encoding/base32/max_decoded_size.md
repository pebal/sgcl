[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md)

# sgcl::encoding::base32::max_decoded_size

```cpp
constexpr size_t max_decoded_size(size_t n) const noexcept;
```

The most bytes a text of `n` characters decodes to: Go's `DecodedLen`, and the size the buffer of
[decode_to](decode_to.md) needs. A padded codec counts every group the text starts as whole,
`(n + 7) / 8 * 5`, so that a buffer of that size holds whatever a text of `n` characters writes before it is
found wrong; without padding it is the bits of `n` characters, `n * 5 / 8`.

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
    auto raw = encoding::base32::standard.without_padding();
    println("{} {}", encoding::base32::standard.max_decoded_size(10), raw.max_decoded_size(10));
}
```

Output:

```text
10 6
```

## See also

- [encoded_size](encoded_size.md): the other way
- [decode_to](decode_to.md): into the caller's buffer
- [sgcl::encoding::base32](../base32.md)
