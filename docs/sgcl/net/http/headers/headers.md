[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::headers

```cpp
headers() noexcept = default;
```

Constructs an empty list of fields. The copy and the move constructors are the implicit ones: a copy is a list of its
own, its fields the same slices of the same strings.

## Parameters

None.

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
    println("{} {}", h.empty(), h.size());
    h.set("Accept", "text/plain");
    net::http::headers copy = h;
    copy.set("Accept", "application/json");
    println("{} | {}", h.get("Accept"), copy.get("Accept"));
}
```

Output:

```text
true 0
text/plain | application/json
```

## See also

- [set](set.md), [add](add.md): the fields put in
- [sgcl::net::http::headers](../headers.md)
