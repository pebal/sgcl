[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::message

```cpp
optional<email> message() const;
```

The message a message/rfc822 (or message/global) part holds, parsed with the message around it.

## Parameters

None.

## Return value

The message, a handle of the tree's own; nothing for any other part.

## Complexity

Constant.

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
        "--b\r\nContent-Type: message/rfc822\r\n\r\n"
        "Subject: forwarded\r\n\r\nold text\r\n--b--\r\n").value();
    auto inner = m.body().parts()[0].message();
    println("{} | {}", inner->subject(), inner->text());
}
```

Output:

```text
forwarded | old text
```

## See also

- [email::attachments](../email/attachments.md)
- [part](README.md)
