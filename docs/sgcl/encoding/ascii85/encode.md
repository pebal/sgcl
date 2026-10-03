[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](README.md)

# sgcl::encoding::ascii85::encode

```cpp
static string encode(const slice<const byte>& data);    // (1)
static string encode(const string& text);               // (2)
template<class T>
static string encode(const T& text);                    // (3)
```

The Ascii85 text of bytes: five characters for every group of four bytes, `z` for four zero bytes, and n + 1
characters for a last group of n. No `<~` and `~>` around it.

1. The text of `data`: a vector, an array, a slice of either, a raw buffer.
2. The text of the bytes of `text`.
3. The same as (2) for a literal, a character array or a `std::string_view`, read where it lies; it takes part
   only for those.

The string is made for the [max_encoded_size](max_encoded_size.md) of the input, written once, in one pass, and
keeps the length the text took.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to encode |
| `text` | the text whose bytes are encoded |

## Return value

The text.

## Complexity

Linear in the size of the input.

## Exceptions

`length_error` when the text could be longer than a string holds (4 G characters).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::ascii85::encode("Hello"));
    vector<byte> data = {byte(0), byte(0), byte(0), byte(0), byte(0xFF)};
    println(encoding::ascii85::encode(data));
}
```

Output:

```text
87cURDZ
zrr
```

## See also

- [decode](decode.md): the bytes of a text
- [encode_to](encode_to.md): into the caller's buffer
- [encoder_to](encoder_to.md): as a stream
- [sgcl::encoding::ascii85](README.md)
