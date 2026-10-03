[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::in_memory

```cpp
static pair<connection, connection> in_memory() noexcept;
```

Makes two connected ends in memory: what one writes, the other reads. Go's `net.Pipe`, with the deadlines,
[close](close.md) and [close_write](close_write.md) as on a socket. A write waits for the reads of the other end
that take it: nothing is buffered, nothing copied twice. For tests of a protocol without sockets, and for a client
and a server in one program.

An end has no [endpoints](local_endpoint.md) and no [path](path.md); its errors name it `pipe`, and the socket
options ([set_no_delay](set_no_delay.md), [set_keep_alive](set_keep_alive.md)) are `EOPNOTSUPP`.

## Parameters

None.

## Return value

The two ends.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

// The protocol under test: a line in, the line quoted back
async::task<> quote(net::connection c) {
    while (auto line = co_await c.async_read_line()) {
        if (!*line) {
            break;
        }
        co_await c.async_write("you said: " + **line + "\n");
    }
    co_await c.async_close();
}

int main() {
    auto [client, server] = net::connection::in_memory();
    auto serving = async::spawn(quote(server));
    client.write("hello\n");
    println("{}", client.read_line()->value());
    client.close_write();
    serving.wait();
}
```

Output:

```text
you said: hello
```

## See also

- [tcp::listen](../tcp/listen.md), [tcp::connect](../tcp/connect.md): the same over a socket
- [sgcl::net::connection](../connection.md)
