[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [form](README.md)

# sgcl::net::http::form::boundary

```cpp
string boundary() const noexcept;
```

The boundary between the parts, Go's `Writer.Boundary`: 32 hexadecimal characters drawn when the form was made, the
same for every copy of it and every body made of it.

## Parameters

None.

## Return value

The boundary.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::form f{{"a", "1"}};
    vector<byte> body = f.reader().value().read_all().value();
    string text(slice<const char>(reinterpret_cast<const char*>(body.data()), body.size()));
    println("{} {}", f.boundary().size(), text.starts_with("--" + f.boundary() + "\r\n"));
}
```

Output:

```text
32 true
```

## See also

- [content_type](content_type.md): the boundary in the request's `Content-Type`
- [form](README.md)
