[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::errc

```cpp
#include "sgcl/net/jsonrpc/error.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    enum class errc {
        parse_error = -32700,
        invalid_request = -32600,
        method_not_found = -32601,
        invalid_params = -32602,
        internal_error = -32603,
        request_cancelled = -32800,
        malformed = 1
    };
}
```

The error codes of JSON-RPC 2.0 (§5.1) by their values, LSP's `request_cancelled`, and the client's own
`malformed`, in the category `"jsonrpc"` ([category](category.md)). A code of the application's (-32000 to -32099,
"server error", or any other) is kept in the error's value as it is.

| Value | Description |
|---|---|
| `parse_error` | -32700, "parse error": the message is not JSON |
| `invalid_request` | -32600, "invalid request": the JSON is not a request |
| `method_not_found` | -32601, "method not found": no method of the name |
| `invalid_params` | -32602, "invalid params": params the method does not take |
| `internal_error` | -32603, "internal error": the server failed |
| `request_cancelled` | -32800, "request cancelled": LSP's: a request the caller cancelled |
| `malformed` | 1, "malformed JSON-RPC message": a response that breaks the specification (the client's own) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/jsonrpc.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::jsonrpc::methods m;
    m.add("add", [](const encoding::json& p) -> expected<encoding::json, net::jsonrpc::error> {
        return encoding::json(p[0].as_int(0) + p[1].as_int(0));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto accepted = async::spawn(l.async_accept());
    net::connection near = net::tcp::connect(l.local_endpoint().to_string()).value();
    net::jsonrpc::peer::options o;
    o.methods = m;
    net::jsonrpc::peer server = net::jsonrpc::peer::connect(accepted.wait().value(), o);
    net::jsonrpc::peer client = net::jsonrpc::peer::connect(near);
    auto r = client.call("nope");
    println("{}", r.error().code() == net::jsonrpc::errc::method_not_found);
}
```

Output:

```text
true
```

## See also

- [error](error.md)
- [category](category.md)
