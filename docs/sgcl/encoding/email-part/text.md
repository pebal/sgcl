[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::text

```cpp
string text() const;
```

The content as text in UTF-8: converted from its charset when [txt](../../txt/README.md) converts it (UTF-8,
ASCII, ISO-8859, the Windows code pages, KOI8, UTF-16 and UTF-32), the bytes as they are for another charset
([charset](charset.md) names it); line breaks LF.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the content.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string raw = "Content-Type: text/plain; charset=windows-1250\r\n"
                 "Content-Transfer-Encoding: quoted-printable\r\n\r\n=8Cl=B9sk\r\n";
    auto m = encoding::email::parse(raw).value();
    print("{}", m.body().text());
}
```

Output:

```text
Śląsk
```

## See also

- [content](content.md)
- [part](README.md)
