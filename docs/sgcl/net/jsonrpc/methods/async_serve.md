[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::methods::async_serve

```cpp
async::task<> async_serve(http::request r, http::response_writer w) const noexcept;
```

The methods as an HTTP handler: a POST's body handled, the response written as `application/json`, 204 when there
is nothing to answer (notifications only), 405 for another HTTP method. The request's stop token is the waiting
methods' (a client that goes away stops them).

## Parameters

| Parameter | Description |
|---|---|
| `r` | the request |
| `w` | its response |


## Return value

A task that ends when the response is written.

## Complexity

Linear in the message.

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
    println("{}", rpc.call("add", encoding::json::array({20, 22})).value().to_string());
    srv.close();
    serving.wait();
}
```

Output:

```text
42
```

## See also

- [client](../client/README.md)
- [http::server](../../http/server/README.md)
- [methods](README.md)
