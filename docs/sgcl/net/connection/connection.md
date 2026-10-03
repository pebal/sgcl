[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::connection

```cpp
connection() noexcept = default;                 // (1)
connection(const connection& other) noexcept;    // (2), implicitly declared
connection(connection&& other) noexcept;         // (3), implicitly declared
```

1. A handle that holds no connection: `!c`. An operation on it is a contract violation (debug builds assert); it is
   given a connection by an assignment.
2. A handle of the same connection as `other`: one socket, one deadline per direction, shared, as two copies of a
   `*net.TCPConn` are in Go.
3. The same, `other` left holding no connection.

A connection with a socket is made by [tcp::connect](../tcp/connect.md),
[unix_domain::connect](../unix_domain/connect.md), [listener::accept](../listener/accept.md), the functions of
[tls](../tls/README.md) and [in_memory](in_memory.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose connection this one shares |

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
    net::connection none;
    println("{}", static_cast<bool>(none));

    auto [a, b] = net::connection::in_memory();
    net::connection same = a;
    same.close();
    println("{} {}", a.is_closed(), same == a);
}
```

Output:

```text
false
true true
```

## See also

- [operator bool](operator_bool.md): whether the handle holds a connection
- [operator==](operator_cmp.md): whether two handles are the same connection
- [sgcl::net::connection](README.md)
