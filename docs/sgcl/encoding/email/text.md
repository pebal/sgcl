[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::text

```cpp
string text() const;
```

The text of the body: its first text/plain part that is not an attachment, walking the tree in order (the first
alternative of a multipart/alternative), converted from its charset to UTF-8 and its line breaks LF. A charset
[txt](../../txt/README.md) does not convert gives the bytes as they are.

## Parameters

None.

## Return value

The text; `""` when the body has none.

## Complexity

Linear in the size of the body.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    auto m = encoding::email::parse(
        "Content-Type: text/plain; charset=iso-8859-2\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
        "Za=BF=F3=B3=E6\r\n").value();
    print("{}", m.text());
}
```

Output:

```text
Zażółć
```

## See also

- [html](html.md)
- [set_text](set_text.md)
- [email](README.md)
