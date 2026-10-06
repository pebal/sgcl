[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::unix_domain

```cpp
#include "sgcl/net/socket.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct unix_domain;
}
```

`sgcl::net::unix_domain` is stream sockets in the file system (`AF_UNIX`): [listen](listen.md) creates
the socket's file and gives a [listener](../listener/README.md), [connect](connect.md) dials the path a listener
made and gives a [connection](../connection/README.md). They are Go's `net.Listen("unix", path)` and `net.Dial("unix", path)`.
A structure of static functions, as [tcp](../tcp/README.md) is; a connection over a unix socket is a connection as
any other, with no [endpoints](../connection/local_endpoint.md) and its [path](../connection/path.md) instead, and it
passes descriptors between processes ([send_descriptors](../connection/send_descriptors.md),
[receive_descriptors](../connection/receive_descriptors.md): `SCM_RIGHTS`, Go's `WriteMsgUnix` with `UnixRights`). The name is
`unix_domain` and not `unix`: `unix` is a macro in the GNU modes of GCC and Clang on Linux.

## Rules

- `listen` creates the socket's file, an error when anything is at the path already, as in Go, and the listener's
  [close](../listener/close.md) removes it.
- A path is at most 103 bytes on macOS, 107 on Linux: `net::errc::invalid_address` past that.
- `connect` dials one address with no race: its blocking form blocks the calling thread wherever it is, a worker
  included, so a task awaits `async_connect`.
- Errors are values, [expected\<T, io::error\>](../../io/error/README.md), the operation `dial unix` or `listen unix` and the
  path.

## Member functions

| Function | Description |
|---|---|
| [connect, async_connect](connect.md) | connects to the socket at a path (static) |
| [listen, async_listen](listen.md) | creates a socket at a path and listens on it (static) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> greet(net::listener l) {
    net::connection c = co_await l.async_accept();
    co_await c.async_write("hello over a unix socket");
    co_await c.async_close();
}

int main() {
    net::listener l = net::unix_domain::listen("app.sock");
    auto server = async::spawn(greet(l));
    net::connection c = net::unix_domain::connect("app.sock");
    println("{}", c.read_all_text().value());
    server.wait();
    c.close();
    l.close();  // removes app.sock
    println("{}", io::exists("app.sock"));
}
```

Output:

```text
hello over a unix socket
false
```

## See also

- [listener](../listener/README.md), [connection](../connection/README.md): what it makes
- [tcp](../tcp/README.md): the same over the network
- [send_descriptors](../connection/send_descriptors.md): descriptors over a unix socket
- `tests/net/socket.cpp`: a unix socket's file
