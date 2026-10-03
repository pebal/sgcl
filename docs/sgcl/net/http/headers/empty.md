[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](README.md)

# sgcl::net::http::headers::empty

```cpp
bool empty() const noexcept;
```

Checks whether the list has no field.

## Parameters

None.

## Return value

`true` when there is no field.

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
    println("{}", h.empty());
    h.set("Accept", "*/*").erase("Accept");
    println("{}", h.empty());
}
```

Output:

```text
true
true
```

## See also

- [size](size.md): the number of fields
- [sgcl::net::http::headers](README.md)
