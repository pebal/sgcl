[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::encode

```cpp
string encode(const slice<const byte>& data) const;    // (1)
string encode(const string& text) const;               // (2)
template<class T>
string encode(const T& text) const;                    // (3)
```

The quoted-printable text of bytes, in lines of at most 76 characters joined by CRLF.

1. The bytes of `data`.
2. The bytes of a text.
3. A literal, a character array, a `std::string_view`: as (2).

In the text form a line break of the input, CRLF or LF alone, is a line break of the output, CRLF, and a CR alone
is `=0D`; in the [binary](binary.md) form CR and LF are `=0D` and `=0A`.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to encode |
| `text` | a text whose bytes are encoded |

## Return value

The text: the characters of 33 to 126 but `=`, the spaces and tabs not at the end of a line, `=XX`, soft breaks `=` CRLF.

## Complexity

Linear in the size of the input.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    auto qp = encoding::quoted_printable::standard;
    println("{}", qp.encode("a=b, trailing space "));
    print("{}\n", qp.encode("two\nlines"));
    println("{}", qp.binary().encode("two\nlines"));
    string long_line = qp.encode(string(100, 'x'));
    println("{}", long_line.view().find("=\r\n"));
}
```

Output:

```text
a=3Db, trailing space=20
two
lines
two=0Alines
75
```

## See also

- [decode](decode.md): the bytes of a text
- [binary](binary.md): CR and LF encoded
- [encoder_to](encoder_to.md): the same as a stream
- [quoted_printable](README.md)
