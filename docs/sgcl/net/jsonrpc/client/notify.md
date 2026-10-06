[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [client](README.md)

# sgcl::net::jsonrpc::client::notify, async_notify

```cpp
expected<void, io::error> notify(const string& method, const json& params = json()) const;                  // (1)
async::task<expected<void, io::error>> async_notify(string method, json params = json()) const noexcept;    // (2)
```

A notification: a POST, the service's 204 (or 200) taken.

`notify` waits on the calling thread; a task awaits `async_notify`.

## Parameters

| Parameter | Description |
|---|---|
| `method` | the notification |
| `params` | an array or an object; null: none sent |


## Return value

Nothing; `net::errc::http_status`, the HTTP client's errors.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    println("{}", bool(rpc.notify("anything")));
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [call](call.md)
- [client](README.md)
