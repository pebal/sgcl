[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::attachments

```cpp
vector<part> attachments() const;
```

The parts a reader of mail offers to save, in their order: those with a disposition of attachment, or with a
file's name and no disposition of inline, and message/rfc822 parts; never the text and the HTML of the body.

## Parameters

None.

## Return value

The [parts](../email-part/README.md), handles of the message's own.

## Complexity

Linear in the number of parts.

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
        "--b\r\nContent-Type: text/plain\r\n\r\nbody\r\n"
        "--b\r\nContent-Type: application/pdf; name=\"a.pdf\"\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nJVBERg==\r\n"
        "--b--\r\n").value();
    for (auto& a : m.attachments()) {
        println("{} {}", a.filename(), string(a.content()));
    }
}
```

Output:

```text
a.pdf %PDF
```

## See also

- [attach](attach.md)
- [part](../email-part/README.md)
- [email](README.md)
