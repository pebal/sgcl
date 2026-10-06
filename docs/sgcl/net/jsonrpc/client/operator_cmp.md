[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [client](README.md)

# sgcl::net::jsonrpc::operator== (sgcl::net::jsonrpc::client)

```cpp
friend bool operator==(const client & a, const client& b) noexcept;
```

Whether two handles are the same client; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same client.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/jsonrpc.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::jsonrpc::methods m;
    m.add("add", [](const encoding::json& p) -> expected<encoding::json, net::jsonrpc::error> {
        return encoding::json(p[0].as_int(0) + p[1].as_int(0));
    });
    net::http::server srv;
    srv.route("POST /rpc", [m](net::http::request r, net::http::response_writer w) { return m.async_serve(r, w); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::jsonrpc::client rpc(string::concat("http://", l.local_endpoint().to_string(), "/rpc"));
    net::jsonrpc::client same = rpc;
    println("{}", same == rpc);
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [client](README.md)
