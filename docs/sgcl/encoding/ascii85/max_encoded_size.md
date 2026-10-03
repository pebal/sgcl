[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md)

# sgcl::encoding::ascii85::max_encoded_size

```cpp
static constexpr size_t max_encoded_size(size_t n) noexcept;
```

The most characters `n` bytes take: five for every group of four, and n % 4 + 1 for a last group that is short.
A bound, not a count: a `z` makes the text fewer characters. It is the size the buffer of
[encode_to](encode_to.md) needs. Go's `MaxEncodedLen` counts the short group as five, `(n + 3) / 4 * 5`.

The arithmetic never wraps: an `n` whose characters no `size_t` can hold gives `SIZE_MAX`, the size no buffer
has.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes |

## Return value

The most characters, or `SIZE_MAX` when it does not fit a `size_t`.

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
    vector<byte> zeros(8);
    println("{}", encoding::ascii85::max_encoded_size(8));
    println("{}", encoding::ascii85::encode(zeros).size());
    println("{}", encoding::ascii85::max_encoded_size(5));
}
```

Output:

```text
10
2
7
```

## See also

- [max_decoded_size](max_decoded_size.md): the other way
- [encode_to](encode_to.md): into the caller's buffer
- [sgcl::encoding::ascii85](../ascii85.md)
