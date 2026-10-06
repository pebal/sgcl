[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [peer](README.md)

# sgcl::net::jsonrpc::peer::wait, async_wait

```cpp
expected<void, io::error> wait() const;                                // (1)
async::task<expected<void, io::error>> async_wait() const noexcept;    // (2)
```

Until the connection ends: nothing for a close of either side, the error that ended it otherwise. A server's
handler of a connection waits on it.

`wait` waits on the calling thread; a task awaits `async_wait`.

## Parameters

None.

## Return value

Nothing; the error that ended the connection.

## Complexity

Constant.

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
    client.close();
    println("{}", bool(server.wait()));
}
```

Output:

```text
true
```

## See also

- [close](close.md)
- [peer](README.md)
