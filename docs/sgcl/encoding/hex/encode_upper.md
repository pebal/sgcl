[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md)

# sgcl::encoding::hex::encode_upper

```cpp
/*(1)*/ static string encode_upper(const slice<const byte>& data);
/*(2)*/ static string encode_upper(const string& text);
/*(3)*/ template<class T>
        static string encode_upper(const T& text);
```

The upper-case hexadecimal digits of bytes, two a byte, as [encode](encode.md) writes the lower-case ones. Go
has no such function; its `%X` of `fmt` writes them.

1. The digits of `data`.
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
    vector<byte> mac = {byte(0x00), byte(0x1A), byte(0x2B), byte(0x3C), byte(0x4D), byte(0x5E)};
    println(encoding::hex::encode_upper(mac));
}
```

Output:

```text
001A2B3C4D5E
```

## See also

- [encode](encode.md): lower-case digits
- [decode](decode.md): the bytes of digits of either case
- [sgcl::encoding::hex](../hex.md)
