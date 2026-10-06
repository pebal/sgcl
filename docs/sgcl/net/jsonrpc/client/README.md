[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md)

# sgcl::net::jsonrpc::client

```cpp
#include "sgcl/net/jsonrpc/client.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::jsonrpc::client` calls a JSON-RPC service over HTTP: each call a POST of `application/json` to the URL,
its response the reply; a batch one POST of all of them. Through an [http::client](../../http/client/README.md):
its pool, TLS and timeouts.

## Rules

- A handle of one word: copies share the HTTP client.
- An HTTP status other than 200 (204 for notifications) is `net::errc::http_status`, its path the status.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | a client of a URL |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another client |
| [call, async_call](call.md) | a call and its result |
| [notify, async_notify](notify.md) | a notification |
| [batch, async_batch](batch.md) | several in one POST |
| [operator bool](operator_bool.md) | whether the handle holds a client |
| [operator==](operator_cmp.md) | whether two handles are the same client |

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
    println("{}", rpc.call("add", encoding::json::array({1, 2})).value().to_string());
    srv.close();
    serving.wait();
}
```

Output:

```text
3
```

## See also

- [async_serve](../methods/async_serve.md)
- [peer](../peer/README.md)
