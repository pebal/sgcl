[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::to_string

```cpp
string to_string() const noexcept;
```

An indented dump of the element, one element a line, two spaces a level, as `openssl asn1parse -i` shows
a structure: the type by its name in X.680 (`SEQUENCE`, `OBJECT IDENTIFIER`, `UTF8String`) or the tag
(`[0]`, `[APPLICATION 1]`, `[PRIVATE 2]`), then the value — a number in decimal, an identifier dotted, a string
quoted with `"`, `\` and the control characters escaped, a time as its text, the bytes in hexadecimal (the first
32 and `...`) with their count, a BIT STRING's count of bits. Written without recursion, however deep. Empty for
[asn1()](asn1.md).

## Parameters

None.

## Return value

The dump, a `\n` after every line.

## Complexity

Linear in the size of the element.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 e = encoding::asn1::sequence({
        encoding::asn1::integer(-5), encoding::asn1::object_identifier(encoding::asn1::oid("2.5.4.3")),
        encoding::asn1::explicit_tag(0, encoding::asn1::printable_string("PL")),
        encoding::asn1::implicit_tag(1, encoding::asn1::octet_string(vector<byte>(3, byte(0xab)))),
        encoding::asn1::utf8_string("say \"hi\"")});
    print(e.to_string());
}
```

Output:

```text
SEQUENCE
  INTEGER -5
  OBJECT IDENTIFIER 2.5.4.3
  [0]
    PrintableString "PL"
  [1] (3 bytes) ababab
  UTF8String "say \"hi\""
```

## See also

- [bytes](bytes.md): the encoding
- [sgcl::encoding::asn1](README.md)
