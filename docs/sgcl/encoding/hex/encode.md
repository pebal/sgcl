[sgcl](../../README.md) › [encoding](../README.md) › [hex](README.md)

# sgcl::encoding::hex::encode

```cpp
static string encode(const slice<const byte>& data);    // (1)
static string encode(const string& text);               // (2)
template<class T>
static string encode(const T& text);                    // (3)
```

The lower-case hexadecimal digits of bytes, two a byte, the high four bits first: Go's `hex.EncodeToString`.

1. The digits of `data`: a vector, an array, a slice of either, a raw buffer.
2. The digits of the bytes of `text`.
3. The same as (2) for a literal, a character array or a `std::string_view`, read where it lies; it takes part
   only for those.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to encode |
| `text` | the text whose bytes are encoded |

## Return value

The digits, twice as many as the bytes.

## Complexity

Linear in the size of the input.

## Exceptions

`length_error` when the digits would be more than a string holds (4 G characters).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> digest = {byte(0xDE), byte(0xAD), byte(0xBE), byte(0xEF)};
    println(encoding::hex::encode(digest));
    println(encoding::hex::encode("Hi!"));
}
```

Output:

```text
deadbeef
486921
```

## See also

- [encode_upper](encode_upper.md): upper-case digits
- [decode](decode.md): the bytes of digits
- [sgcl::encoding::hex](README.md)
