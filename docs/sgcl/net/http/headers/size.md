[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::size

```cpp
size_t size() const noexcept;
```

Returns the number of fields, each field counted, a name that comes twice twice.

## Parameters

None.

## Return value

The number of fields.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Set-Cookie", "a=1").add("Set-Cookie", "b=2").add("Accept", "*/*");
    println("{}", h.size());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md): whether there is no field
- [sgcl::net::http::headers](../headers.md)
