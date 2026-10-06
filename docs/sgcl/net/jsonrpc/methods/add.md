[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::methods::add

```cpp
methods& add(const string& name, function<expected<json, error>(const json& params)> f);
```

A method that answers at once: its params in, its result or an [error](../error.md) object out. A method of the name
there already is replaced.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the method's name |
| `f` | the handler: the params (an array, an object, or null), the result or an error object |


## Return value

The table.

## Complexity

Constant.

## Exceptions

What the copy of `f` throws.

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
    m.add("divide", [](const encoding::json& p) -> expected<encoding::json, net::jsonrpc::error> {
        if (p[1].as_int(0) == 0) {
            return unexpected(net::jsonrpc::error(-32000, "division by zero"));
        }
        return encoding::json(p[0].as_int(0) / p[1].as_int(0));
    });
    println("{}", m.handle(R"({"jsonrpc":"2.0","method":"divide","params":[7,2],"id":1})").value());
    println("{}", m.handle(R"({"jsonrpc":"2.0","method":"divide","params":[7,0],"id":2})").value());
}
```

Output:

```text
{"jsonrpc":"2.0","id":1,"result":3}
{"jsonrpc":"2.0","id":2,"error":{"code":-32000,"message":"division by zero"}}
```

## See also

- [add_task](add_task.md)
- [error](../error.md)
- [methods](README.md)
