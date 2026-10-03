[sgcl](../../README.md) › [net](../README.md) › [listener](../listener.md)

# sgcl::net::listener::path

```cpp
string path() const noexcept;
```

Returns the path of a unix listener, the one [unix_domain::listen](../unix_domain/listen.md) was given; empty for
TCP. The connections the listener accepts have the same [path](../connection/path.md).

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
    println("{} {}", l.path(), io::exists("app.sock"));
    l.close();  // removes the file
    println("{}", io::exists("app.sock"));

    net::listener t = net::tcp::listen("127.0.0.1:0");
    println("{}", t.path().empty());
}
```

Output:

```text
app.sock true
false
true
```

## See also

- [local_endpoint](local_endpoint.md): a TCP listener's address
- [unix_domain](../unix_domain.md): stream sockets in the file system
- [sgcl::net::listener](../listener.md)
