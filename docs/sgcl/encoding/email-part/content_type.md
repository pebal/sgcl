[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::content_type

```cpp
string content_type() const;
```

The media type of Content-Type, lower case, without its parameters: `text/plain` when there is no Content-Type or
it cannot be read (RFC 2045 §5.2), `message/rfc822` for a part of a multipart/digest without one.

## Parameters

None.

## Return value

The type.

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
    println("{} {}", m.body().content_type(), m.body().parts()[1].content_type());
}
```

Output:

```text
multipart/mixed application/pdf
```

## See also

- [param](param.md)
- [part](README.md)
