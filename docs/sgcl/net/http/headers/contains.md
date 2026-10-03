[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::contains

```cpp
bool contains(const string& name) const noexcept;
```

Checks whether a field named `name` is in the list, in any ASCII case: what tells a field with an empty value from no
field, which [get](get.md) gives as the same `""`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field, in any case |

## Return value

`true` when at least one field has the name.

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
    h.set("X-Empty", "");
    println("{} {}", h.contains("x-empty"), h.contains("X-Missing"));
}
```

Output:

```text
true false
```

## See also

- [get](get.md): the first value
- [sgcl::net::http::headers](../headers.md)
