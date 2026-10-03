[sgcl](../../README.md) › [encoding](../README.md) › [base64](README.md)

# sgcl::encoding::base64::encode

```cpp
string encode(const slice<const byte>& data) const;    // (1)
string encode(const string& text) const;               // (2)
template<class T>
string encode(const T& text) const;                    // (3)
```

The text of bytes in the codec's alphabet, the last group padded when the codec is
[padded](padded.md): Go's `EncodeToString`.

1. The text of `data`: a vector, an array, a slice of either, a raw buffer.
2. The text of the bytes of `text`: `"user:password"` for Basic authentication.
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
    vector<byte> key = {byte(0xFB), byte(0xFF), byte(0x00)};
    println(encoding::base64::standard.encode(key));
    println(encoding::base64::url.encode(key));
    println(encoding::base64::standard.encode("ala:sekret"));
    println(encoding::base64::raw_standard.encode(string("hi")));
}
```

Output:

```text
+/8A
-_8A
YWxhOnNla3JldA==
aGk
```

## See also

- [decode](decode.md): the bytes of a text
- [encode_to](encode_to.md): into the caller's buffer
- [encoder_to](encoder_to.md): as a stream
- [sgcl::encoding::base64](README.md)
