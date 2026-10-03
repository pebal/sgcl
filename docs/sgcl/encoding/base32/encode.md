[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md)

# sgcl::encoding::base32::encode

```cpp
/*(1)*/ string encode(const slice<const byte>& data) const;
/*(2)*/ string encode(const string& text) const;
/*(3)*/ template<class T>
        string encode(const T& text) const;
```

The text of bytes in the codec's alphabet, the last group padded to eight characters when the codec is
[padded](padded.md): Go's `EncodeToString`.

1. The text of `data`: a vector, an array, a slice of either, a raw buffer.
2. The text of the bytes of `text`.
3. The same as (2) for a literal, a character array or a `std::string_view`, read where it lies; it takes part
   only for those.

The string is made at the size the text takes and written once, in one pass.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to encode |
| `text` | the text whose bytes are encoded |

## Return value

The text: [encoded_size](encoded_size.md) of the input's size characters.

## Complexity

Linear in the size of the input.

## Exceptions

`length_error` when the text would be longer than a string holds (4 G characters).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> id = {byte(0x00), byte(0x01), byte(0x02), byte(0xFF)};
    println(encoding::base32::standard.encode(id));
    println(encoding::base32::hex.encode(id));
    println(encoding::base32::standard.encode("fooba"));
}
```

Output:

```text
AAAQF7Y=
000G5VO=
MZXW6YTB
```

## See also

- [decode](decode.md): the bytes of a text
- [encode_to](encode_to.md): into the caller's buffer
- [encoder_to](encoder_to.md): as a stream
- [sgcl::encoding::base32](../base32.md)
