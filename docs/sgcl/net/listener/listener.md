[sgcl](../../README.md) › [net](../README.md) › [listener](README.md)

# sgcl::net::listener::listener

```cpp
listener() noexcept = default;               // (1)
listener(const listener& other) noexcept;    // (2), implicitly declared
listener(listener&& other) noexcept;         // (3), implicitly declared
```

1. A handle that holds no listener: `!l`. An operation on it is a contract violation (debug builds assert); it is
   given a listener by an assignment.
2. A handle of the same listener as `other`: one socket, shared; a close through either closes it.
3. The same, `other` left holding no listener.

A listener with a socket is made by [tcp::listen](../tcp/listen.md), [unix_domain::listen](../unix_domain/listen.md)
and [tls::listen](../tls/listen.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose listener this one shares |

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
    net::listener none;
    println("{}", static_cast<bool>(none));

    net::listener l = net::tcp::listen("127.0.0.1:0");
    net::listener same = l;
    same.close();
    println("{} {}", l.is_closed(), same == l);
}
```

Output:

```text
false
true true
```

## See also

- [operator bool](operator_bool.md): whether the handle holds a listener
- [operator==](operator_cmp.md): whether two handles are the same listener
- [sgcl::net::listener](README.md)
