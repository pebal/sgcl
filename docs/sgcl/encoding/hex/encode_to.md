[sgcl](../../README.md) › [encoding](../README.md) › [hex](README.md)

# sgcl::encoding::hex::encode_to

```cpp
static size_t encode_to(const slice<char>& out, const slice<const byte>& data);
```

The lower-case digits of `data` written into the caller's buffer, nothing allocated: Go's `hex.Encode`. `out`
holds at least [encoded_size](encoded_size.md)`(data.size())` characters; a smaller one is a mistake in the
program, refused before anything is written. The characters past the digits are left as they were. `out` does not
overlap `data`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the digits are written into |
| `data` | the bytes to encode |

## Return value

The number of digits written, `2 * data.size()`.

## Complexity

Linear in the size of `data`.

## Exceptions

`length_error` when `out` is smaller than `encoded_size(data.size())`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 3> color = {byte(0x1E), byte(0x90), byte(0xFF)};
    array<char, 7> css = {};
    css[0] = '#';
    size_t n = encoding::hex::encode_to(css.as_slice(1), color);
    println("{} {}", n, string(css.data(), 1 + n));
}
```

Output:

```text
6 #1e90ff
```

## See also

- [encoded_size](encoded_size.md): the size the buffer needs
- [encode](encode.md): into a string of its own
- [decode_to](decode_to.md): the other way
- [sgcl::encoding::hex](README.md)
