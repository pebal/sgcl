[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::response_recorder

```cpp
response_recorder() noexcept;
```

Constructs an empty recorder: status 200, no fields, no body. A copy, made by the copy constructor, is the same
recorder, as a copy of a handle is.

## Parameters

None.

## Complexity

Constant: the writer's state is one managed object.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::response_recorder rec;
    println("{} {} {} {}", rec.status(), rec.headers().size(), rec.body().empty(), rec.flushes());
    net::http::response_recorder same = rec;
    same.writer().write("through the copy");
    println("{}", rec.body());
}
```

Output:

```text
200 0 true 0
through the copy
```

## See also

- [writer](writer.md)
- [sgcl::net::http::response_recorder](README.md)
