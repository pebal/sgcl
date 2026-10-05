[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::header

```cpp
string header(const string& name) const;
```

The first value of the field `name` (its case aside), unfolded, its encoded words decoded.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

The value, or `""` when there is none.

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
    println("{}", m.body().parts()[1].header("content-transfer-encoding"));
}
```

Output:

```text
base64
```

## See also

- [set_header](set_header.md)
- [part](README.md)
