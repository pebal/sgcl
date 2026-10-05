[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::is_attachment

```cpp
bool is_attachment() const;
```

Whether a reader of mail offers the part to save: a leaf with a disposition of attachment, or with a file's name
and no disposition of inline.

## Parameters

None.

## Return value

`true` or `false`.

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
    println("{} {}", m.body().parts()[1].is_attachment(), m.body().parts()[0].is_attachment());
}
```

Output:

```text
true false
```

## See also

- [email::attachments](../email/attachments.md)
- [part](README.md)
