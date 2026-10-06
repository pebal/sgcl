[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [peer](README.md)

# sgcl::net::jsonrpc::peer::connect

```cpp
static peer connect(const net::connection& c);                       // (1)
static peer connect(const net::connection& c, const options& o);     // (2)
static peer connect(const http::websocket& ws);                      // (3)
static peer connect(const http::websocket& ws, const options& o);    // (4)
```

JSON-RPC over the connection, read from now on: the other side's requests served by the options' methods.

- (1–2) Over a byte stream, framed as the options say (LSP's `Content-Length` by default).
- (3–4) Over a WebSocket, a message of text each.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the connection |
| `ws` | the WebSocket |
| `o` | the framing, the methods, the cancel method, the size limit |


## Return value

The peer.

## Complexity

Constant.

## Exceptions

None.

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
    o.framing = net::jsonrpc::framing::line;
    o.methods = m;
    net::jsonrpc::peer server = net::jsonrpc::peer::connect(accepted.wait().value(), o);
    net::jsonrpc::peer client = net::jsonrpc::peer::connect(near, {.framing = net::jsonrpc::framing::line});
    println("{}", client.call("add", encoding::json::array({1, 2})).value().to_string());
}
```

Output:

```text
3
```

## See also

- [options](../peer-options.md)
- [peer](README.md)
