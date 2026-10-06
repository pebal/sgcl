[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::error

```cpp
#include "sgcl/net/jsonrpc/error.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    struct error {
        int code = 0;
        string message;
        optional<json> data;

        error();
        error(int code, const string& message, optional<json> data = nullopt);
        error(errc code, const string& message, optional<json> data = nullopt);
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::jsonrpc::error` is an error object of JSON-RPC 2.0 (§5.1): what a handler returns to refuse a call, and
what [error_of](error_of.md) reads back from a call's error. The codes from -32768 to -32000 are the specification's
([errc](errc.md)); the others are the application's.

## Member objects

| Member | Description |
|---|---|
| `code` | the code |
| `message` | a short description |
| `data` | anything more; none: no data member |

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
    m.add("check", [](const encoding::json&) -> expected<encoding::json, net::jsonrpc::error> {
        return unexpected(net::jsonrpc::error(net::jsonrpc::errc::invalid_params, "need a name"));
    });
    println("{}", m.handle(R"({"jsonrpc":"2.0","method":"check","id":1})").value());
}
```

Output:

```text
{"jsonrpc":"2.0","id":1,"error":{"code":-32602,"message":"need a name"}}
```

## See also

- [error_of](error_of.md)
- [errc](errc.md)
