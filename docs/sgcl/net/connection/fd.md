[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::fd

```cpp
int fd() const noexcept;
```

The socket's descriptor, for a call of the system the library does not make: a socket passed to another process
([send_descriptors](send_descriptors.md) of another connection), an option net does not set. The connection
keeps owning it: it is not closed by the caller, and not used after [close](close.md).

## Parameters

None.

## Return value

The descriptor; `-1` for a connection without a socket of its own (TLS, [in_memory](in_memory.md)) and for a closed
one.

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
    net::listener l = net::unix_domain::listen("fd.sock");
    net::connection c = net::unix_domain::connect("fd.sock");
    println("{}", c.fd() >= 0);
    println("{}", net::connection::in_memory().first.fd());
    c.close();
    println("{}", c.fd());
    l.close();
}
```

Output:

```text
true
-1
-1
```

## See also

- [is_closed](is_closed.md)
- [send_descriptors](send_descriptors.md)
- [sgcl::net::connection](README.md)
