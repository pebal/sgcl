[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::encode

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    vector<byte> encode(const string& text, encoding to) noexcept;                     // (1)
    optional<vector<byte>> encode(const string& text, const string& name) noexcept;    // (2)
}
```

Returns the text `text` as bytes in the encoding `to`. A character the encoding cannot write is a question mark,
`'?'`, which is what every library that does this has always done and what a reader can at least see; an invalid
sequence of UTF-8 in `text` is a `U+FFFD`, which a single byte encoding writes as `'?'` too. No byte order mark is
written.

1. In the encoding `to`.
2. In the encoding named by `name`, as [encoding_from_name](encoding_from_name.md) reads it, then as (1); nothing
   when nobody knows the name, as [decode](decode.md) by a name.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `to` | the encoding |
| `name` | the name of the encoding, in any case, with or without its dashes |

## Return value

1. The bytes.
2. The bytes, or an empty `optional` when the name is none of an encoding.

## Complexity

Linear in the length of the text: the vector is sized once from it and what is not used given back. Into UTF-8,
the text is first checked: a valid one is copied as it is, one that is not valid is given three bytes a byte, the
size of a `U+FFFD`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{::#04x}", txt::encode("日", txt::encoding::iso8859_2));
    println("{::#04x}", txt::encode("Ł", txt::encoding::iso8859_2));
    println("{} bytes", txt::encode("a😀", txt::encoding::utf16be).size());
    println("{::#04x}", txt::encode("Ł", "Latin2").value());
    println("{}", txt::encode("Ł", "klingon").has_value());
}
```

Output:

```text
[0x3f]
[0xa3]
6 bytes
[0xa3]
false
```

## See also

- [decode](decode.md): the way back
- [to_utf16](to_utf16.md), [to_utf32](to_utf32.md): units rather than bytes
- [sgcl::txt](README.md)
