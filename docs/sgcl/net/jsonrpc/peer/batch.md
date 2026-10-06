[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [peer](README.md)

# sgcl::net::jsonrpc::peer::batch, async_batch

```cpp
expected<vector<expected<json, io::error>>, io::error> batch(const vector<batch_entry>& entries,                        // (1)
                                                           const call_options& o = {}) const;
async::task<expected<vector<expected<json, io::error>>, io::error>> async_batch(vector<batch_entry> entries,            // (2)
                                                                                call_options o = {}) const noexcept;
```

A batch (§6): the calls and notifications in one message, the calls' outcomes in their order (the notifications
have none). The batch's own failure — the connection, the timeout — is the error.

`batch` waits on the calling thread; a task awaits `async_batch`.

## Parameters

| Parameter | Description |
|---|---|
| `entries` | the calls and notifications |
| `o` | the timeout, for all of them |


## Return value

The calls' outcomes; the connection's end, `ETIMEDOUT`.

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
    auto r = client.batch({{"add", encoding::json::array({1, 2})}, {"nope"}, {"add", encoding::json::array({3, 4})}}).value();
    for (auto& one : r) {
        println("{}", one ? one->to_string() : string("error"));
    }
}
```

Output:

```text
3
error
7
```

## See also

- [batch_entry](../batch_entry.md)
- [peer](README.md)
