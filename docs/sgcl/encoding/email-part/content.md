[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::content

```cpp
vector<byte> content() const;
```

The content with its transfer encoding undone (base64, quoted-printable; 7bit, 8bit and binary as they are); a
message/rfc822 part's is the text of the message it holds. Empty for a multipart.

## Parameters

None.

## Return value

The bytes.

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
    auto m = encoding::email::parse(
        "Content-Type: multipart/mixed; boundary=b\r\n\r\n"
        "--b\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nHello\r\n"
        "--b\r\nContent-Type: application/pdf; name=a.pdf\r\n"
        "Content-Disposition: attachment; filename*=utf-8''r%C3%A9sum%C3%A9.pdf\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nJVBERg==\r\n"
        "--b--\r\n").value();
    println("{}", string(m.body().parts()[1].content()));
}
```

Output:

```text
%PDF
```

## See also

- [text](text.md)
- [part](README.md)
