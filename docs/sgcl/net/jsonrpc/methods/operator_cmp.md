[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::operator== (sgcl::net::jsonrpc::methods)

```cpp
friend bool operator==(const methods & a, const methods& b) noexcept;
```

Whether two handles are the same table; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same table.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/jsonrpc.h"

using namespace sgcl;

int main() {
    net::jsonrpc::methods m;
    m.add("add", [](const encoding::json& p) -> expected<encoding::json, net::jsonrpc::error> {
        return encoding::json(p[0].as_int(0) + p[1].as_int(0));
    });
    net::jsonrpc::methods same = m, other;
    println("{} {}", same == m, other == m);
}
```

Output:

```text
true false
```

## See also

- [methods](README.md)
