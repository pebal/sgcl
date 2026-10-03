[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md)

# sgcl::encoding::ascii85::max_decoded_size

```cpp
static constexpr size_t max_decoded_size(size_t n) noexcept;
```

The most bytes a text of `n` characters can decode to, `4 * n`: every character a `z`. A bound for any text of
that length, and so a size that always serves the buffer of [decode_to](decode_to.md), which asks only for the
bound of the text it is given. Past what a `size_t` holds it is `SIZE_MAX`.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of characters |

## Return value

`4 * n`, or `SIZE_MAX` when it does not fit a `size_t`.

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
    println("{}", encoding::ascii85::max_decoded_size(3));
    println("{}", encoding::ascii85::decode("zzz")->size());
}
```

Output:

```text
12
12
```

## See also

- [max_encoded_size](max_encoded_size.md): the other way
- [decode_to](decode_to.md): into the caller's buffer
- [sgcl::encoding::ascii85](../ascii85.md)
