[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::set_filename

```cpp
part& set_filename(const string& name);
```

Content-Disposition with the file's name: `attachment` unless the part has another disposition, the name a token,
a quoted string, or by RFC 2231 in UTF-8 when it is not ASCII (in sections when long).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the file |

## Return value

`*this`.

## Complexity

Linear in the size of the name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::part p("application/pdf", vector<byte>(3, byte('x')));
    p.set_filename("zażółć.pdf");
    println("{}", p.headers()[1].second);
    println("{} {}", p.filename(), p.disposition());
}
```

Output:

```text
attachment; filename*=utf-8''za%C5%BC%C3%B3%C5%82%C4%87.pdf
zażółć.pdf attachment
```

## See also

- [filename](filename.md)
- [part](README.md)
