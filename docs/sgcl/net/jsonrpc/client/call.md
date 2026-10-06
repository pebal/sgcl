[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [client](README.md)

# sgcl::net::jsonrpc::client::call, async_call

```cpp
expected<json, io::error> call(const string& method, const json& params = json()) const;                  // (1)
async::task<expected<json, io::error>> async_call(string method, json params = json()) const noexcept;    // (2)
```

A call and its result: a POST of the request, the response read.

`call` waits on the calling thread; a task awaits `async_call`.

## Parameters

| Parameter | Description |
|---|---|
| `method` | the method |
| `params` | an array or an object; null: none sent |


## Return value

The result; the error object of a refusal as an error of the category `"jsonrpc"`, `net::errc::http_status`, the HTTP client's errors.

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
    auto missing = rpc.call("nope");
    println("{}", missing.error().code() == net::jsonrpc::errc::method_not_found);
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [batch](batch.md)
- [client](README.md)
