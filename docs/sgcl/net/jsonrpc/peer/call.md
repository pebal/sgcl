[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [peer](README.md)

# sgcl::net::jsonrpc::peer::call, async_call

```cpp
expected<json, io::error> call(const string& method, const json& params = json(),         // (1)
                               const call_options& o = {}) const;
async::task<expected<json, io::error>> async_call(string method, json params = json(),    // (2)
                                                  call_options o = {}) const noexcept;
```

A call and its result. A refusal of the other side is its error object as an error of the category `"jsonrpc"`
([error_of](../error_of.md) reads it back). Past the [options](../call_options.md)' timeout the call fails with
`ETIMEDOUT`; when their stop token is stopped, with `ECANCELED` at once, the other side sent the cancel method.

`call` waits on the calling thread; a task awaits `async_call`.

## Parameters

| Parameter | Description |
|---|---|
| `method` | the method |
| `params` | an array or an object; null: none sent |
| `o` | the timeout and the stop token |


## Return value

The result; the error object of a refusal, `ETIMEDOUT`, `ECANCELED`, the connection's end.

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
    m.add("fail", [](const encoding::json&) -> expected<encoding::json, net::jsonrpc::error> {
        return unexpected(net::jsonrpc::error(-32001, "busy", encoding::json::object({{"retry_after", encoding::json(5)}})));
    });
    auto r = client.call("fail");
    auto e = net::jsonrpc::error_of(r.error()).value();
    println("{} {} {}", e.code, e.message, e.data->to_string());
}
```

Output:

```text
-32001 busy {"retry_after":5}
```

## See also

- [notify](notify.md)
- [call_options](../call_options.md)
- [peer](README.md)
