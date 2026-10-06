[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [client](README.md)

# sgcl::net::jsonrpc::client::client

```cpp
client() noexcept;                                   // (1)
explicit client(const string& url);                  // (2)
client(const string& url, const http::client& h);    // (3)
client(const client& other) noexcept;                // (4)
```

1. No client: `operator bool` is false; an operation on it is a contract violation.
2. A client of the URL through an HTTP client of the default settings.
3. The same through the program's HTTP client (its TLS, proxy, timeouts and pool).
4. The same client as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the service's URL |
| `h` | the HTTP client |
| `other` | the handle copied |


## Return value

None.

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
    net::jsonrpc::client other(string::concat("http://", l.local_endpoint().to_string(), "/rpc"), net::http::client());
    println("{}", other.call("add", encoding::json::array({5, 5})).value().to_string());
    srv.close();
    serving.wait();
}
```

Output:

```text
10
```

## See also

- [call](call.md)
- [client](README.md)
