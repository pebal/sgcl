[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::decode

```cpp
expected<vector<byte>, error> decode(const string& text) const;    // (1)
template<class T>
expected<vector<byte>, error> decode(const T& text) const;         // (2)
```

The bytes of a quoted-printable text: `=XX` in either case, a `=` at the end of a line (white space after it
allowed) a soft break, the white space at the end of a line dropped, a line break kept as it came.

1. A string.
2. A literal, a character array, a `std::string_view`: read where it lies.

A strict codec refuses a `=` that is neither an escape nor a soft break; a [lenient](lenient.md) one keeps it as
it is.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to decode |

## Return value

The bytes, or the [error](../error/README.md): `errc::invalid_escape` at the offset of a `=` followed by neither two hexadecimal digits nor the end of a line, `errc::unexpected_end` for an escape the text ends in.

## Complexity

Linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    auto qp = encoding::quoted_printable::standard;
    println("{}", string(qp.decode("Gr=c3=bc=C3=9Fe soft=\r\nbreak").value()));
    println("{}", qp.decode("100% =ZZ").error().message());
    println("{}", string(qp.lenient().decode("100% =ZZ").value()));
}
```

Output:

```text
Grüße softbreak
offset 5: invalid quoted-printable escape
100% =ZZ
```

## See also

- [encode](encode.md)
- [lenient](lenient.md): the robust decoding
- [decoder_from](decoder_from.md): the same as a stream
- [quoted_printable](README.md)
