[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::disposition

```cpp
string disposition() const;
```

The disposition type of Content-Disposition (RFC 2183), lower case.

## Parameters

None.

## Return value

`"attachment"`, `"inline"`, another type, or `""` when there is no Content-Disposition.

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
    println("{} '{}'", m.body().parts()[1].disposition(), m.body().parts()[0].disposition());
}
```

Output:

```text
attachment ''
```

## See also

- [is_attachment](is_attachment.md)
- [part](README.md)
