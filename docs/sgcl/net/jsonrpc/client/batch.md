[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [client](README.md)

# sgcl::net::jsonrpc::client::batch, async_batch

```cpp
expected<vector<expected<json, io::error>>, io::error> batch(const vector<batch_entry>& entries) const;                         // (1)
async::task<expected<vector<expected<json, io::error>>, io::error>> async_batch(vector<batch_entry> entries) const noexcept;    // (2)
```

A batch in one POST: the outcomes of its calls in their order.

`batch` waits on the calling thread; a task awaits `async_batch`.

## Parameters

| Parameter | Description |
|---|---|
| `entries` | the calls and notifications |


## Return value

The calls' outcomes; the POST's own failure.

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
    auto r = rpc.batch({{"add", encoding::json::array({1, 1})}, {"add", encoding::json::array({2, 2})}}).value();
    println("{} {}", r[0]->to_string(), r[1]->to_string());
    srv.close();
    serving.wait();
}
```

Output:

```text
2 4
```

## See also

- [batch_entry](../batch_entry.md)
- [client](README.md)
