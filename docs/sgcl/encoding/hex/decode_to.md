[sgcl](../../README.md) › [encoding](../README.md) › [hex](README.md)

# sgcl::encoding::hex::decode_to

```cpp
static expected<size_t, error> decode_to(const slice<byte>& out, const string& text);    // (1)
static expected<size_t, error> decode_to(const slice<byte>& out,                         // (2)
                                         const slice<const char>& text);
template<class T>
static expected<size_t, error> decode_to(const slice<byte>& out, const T& text);         // (3)
```

The bytes of digits written into the caller's buffer, nothing allocated: Go's `hex.Decode`. The digits are read
as [decode](decode.md) reads them. `out` holds at least [max_decoded_size](max_decoded_size.md)`(text.size())`
bytes; a smaller one is a mistake in the program, refused before anything is read.

1. The digits of a string.
2. Characters read where they lie, no string made: a file's bytes, a secret's.
3. A literal, a character array or a `std::string_view`, read where it lies, a literal to its first `'\0'`; it
   takes part only for those.

Digits found wrong have had the bytes before the error written into `out`. `out` does not overlap the text.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the bytes are written into |
| `text` | the digits to decode |

## Return value

The number of bytes written, or the [error](../error/README.md) with its offset, as [decode](decode.md)'s.

## Complexity

Linear in the size of `text`.

## Exceptions

`length_error` when `out` is smaller than `max_decoded_size(text.size())`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 3> color = {};
    auto n = encoding::hex::decode_to(color, "1E90FF");
    println("{} {} {} {}", *n, int(color[0]), int(color[1]), int(color[2]));
    println(encoding::hex::decode_to(color, "1E90FG").error().message());
}
```

Output:

```text
3 30 144 255
offset 5: invalid character 'G'
```

## See also

- [max_decoded_size](max_decoded_size.md): the size the buffer needs
- [decode](decode.md): into a vector of its own
- [encode_to](encode_to.md): the other way
- [sgcl::encoding::hex](README.md)
