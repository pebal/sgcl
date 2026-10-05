[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::param

```cpp
string param(const string& name) const;
```

A parameter of Content-Type by its name (its case aside), decoded: a quoted string's escapes taken, RFC 2231's
continuations joined and their charset converted, an encoded word in a quoted value decoded.

## Parameters

None.

## Return value

The value; `""` when there is none.

## Complexity

Linear in the size of the head.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto m = encoding::email::parse(
        "Content-Type: multipart/mixed; boundary=b\r\n\r\n"
        "--b\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nHello\r\n"
        "--b\r\nContent-Type: application/pdf; name=a.pdf\r\n"
        "Content-Disposition: attachment; filename*=utf-8''r%C3%A9sum%C3%A9.pdf\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nJVBERg==\r\n"
        "--b--\r\n").value();
    println("{} {}", m.body().param("BOUNDARY"), m.body().parts()[1].param("name"));
}
```

Output:

```text
b a.pdf
```

## See also

- [charset](charset.md)
- [content_type](content_type.md)
- [part](README.md)
