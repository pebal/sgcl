[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](README.md)

# sgcl::encoding::ascii85::encode_to

```cpp
static size_t encode_to(const slice<char>& out, const slice<const byte>& data);
```

The text of `data` written into the caller's buffer, nothing allocated: Go's `ascii85.Encode`. `out` holds at
least [max_encoded_size](max_encoded_size.md)`(data.size())` characters, the bound; a smaller one is a mistake in
the program, refused before anything is written. The text may be shorter than the bound, a `z` one character
for four bytes; the characters past it are left as they were. `out` does not
overlap `data`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the text is written into |
| `data` | the bytes to encode |

## Return value

The number of characters written.

## Complexity

Linear in the size of `data`.

## Exceptions

`length_error` when `out` is smaller than `max_encoded_size(data.size())`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 8> data = {byte(0), byte(0), byte(0), byte(0),
                           byte('H'), byte('e'), byte('l'), byte('l')};
    array<char, 10> text = {};
    size_t n = encoding::ascii85::encode_to(text, data);
    println("{} {}", n, string(text.data(), n));
}
```

Output:

```text
6 z87cUR
```

## See also

- [max_encoded_size](max_encoded_size.md): the size the buffer needs
- [encode](encode.md): into a string of its own
- [decode_to](decode_to.md): the other way
- [sgcl::encoding::ascii85](README.md)
