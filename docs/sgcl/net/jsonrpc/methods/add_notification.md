[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::methods::add_notification

```cpp
methods& add_notification(const string& name, function<void(const json& params)> f);
```

A notification: its params in, nothing answered. A method of the name there already is replaced.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the notification's name |
| `f` | the handler of its params |


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
    int seen = 0;
    m.add_notification("tick", [&seen](const encoding::json&) { ++seen; });
    println("[{}] {}", m.handle(R"({"jsonrpc":"2.0","method":"tick"})").value_or("none"), seen);
}
```

Output:

```text
[none] 1
```

## See also

- [notify](../peer/notify.md)
- [methods](README.md)
