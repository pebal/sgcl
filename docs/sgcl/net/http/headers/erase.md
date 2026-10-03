[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](README.md)

# sgcl::net::http::headers::erase

```cpp
headers& erase(const string& name) noexcept;
```

Drops every field named `name`, found in any ASCII case, Go's `Header.Del`. The other fields keep their order. A name
that is not there changes nothing.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the fields, in any case |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Cookie", "a=1").add("Accept", "*/*").add("cookie", "b=2");
    h.erase("COOKIE").erase("X-Missing");
    println("{} {}", h.size(), h.contains("Cookie"));
}
```

Output:

```text
1 false
```

## See also

- [set](set.md): one field of the name left
- [sgcl::net::http::headers](README.md)
