[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::path

```cpp
string path() const noexcept;
```

Returns the path of a unix socket, the one [unix_domain::connect](../unix_domain/connect.md) dialed or the
listener's of an accepted connection; empty for TCP and for a pair in memory, whose ends have
[endpoints](local_endpoint.md) or nothing.

## Parameters

None.

## Return value

The path, or an empty string.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::listener l = net::unix_domain::listen("app.sock");
    net::connection client = net::unix_domain::connect("app.sock");
    net::connection server = l.accept();
    println("{} {}", client.path(), server.path());
    println("{}", client.local_endpoint().is_valid());
    client.close();
    server.close();
    l.close();
}
```

Output:

```text
app.sock app.sock
false
```

## See also

- [local_endpoint](local_endpoint.md), [remote_endpoint](remote_endpoint.md): the addresses of a TCP connection
- [unix_domain](../unix_domain.md): stream sockets in the file system
- [sgcl::net::connection](../connection.md)
