[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::methods::remove

```cpp
methods& remove(const string& name);
```

The method or notification of the name taken out; a call of it then gets `errc::method_not_found`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |


## Return value

The table.

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
    m.remove("add");
    println("{}", m.handle(R"({"jsonrpc":"2.0","method":"add","params":[1,2],"id":1})").value());
}
```

Output:

```text
{"jsonrpc":"2.0","id":1,"error":{"code":-32601,"message":"Method not found"}}
```

## See also

- [add](add.md)
- [methods](README.md)
