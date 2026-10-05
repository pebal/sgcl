[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::parts

```cpp
vector<part> parts() const;
```

The parts of a multipart, in their order; handles of the message's own.

## Parameters

None.

## Return value

The parts; empty for a leaf.

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
        "--b\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nHello\r\n"
        "--b\r\nContent-Type: application/pdf; name=a.pdf\r\n"
        "Content-Disposition: attachment; filename*=utf-8''r%C3%A9sum%C3%A9.pdf\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nJVBERg==\r\n"
        "--b--\r\n").value();
    for (auto& p : m.body().parts()) {
        println("{}", p.content_type());
    }
}
```

Output:

```text
text/plain
application/pdf
```

## See also

- [add](add.md)
- [part](README.md)
