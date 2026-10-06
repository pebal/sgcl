[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::methods::methods

```cpp
methods();                                 // (1)
methods(const methods& other) noexcept;    // (2)
```

1. An empty table.
2. The same table as `other`: a method added to one is in both.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle copied |


## Return value

None.

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
    net::jsonrpc::methods same = m;
    same.add("zero", [](const encoding::json&) -> expected<encoding::json, net::jsonrpc::error> { return encoding::json(0); });
    println("{}", m.handle(R"({"jsonrpc":"2.0","method":"zero","id":1})").value());
}
```

Output:

```text
{"jsonrpc":"2.0","id":1,"result":0}
```

## See also

- [add](add.md)
- [methods](README.md)
