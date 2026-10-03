[sgcl](../../README.md) › [net](../README.md) › [listener](../listener.md)

# sgcl::net::listener::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a listener: `false` for one made by the default constructor, `true` for any made by
the module, closed or not. A closed listener is told by [is_closed](is_closed.md).

## Parameters

None.

## Return value

`true` when the handle holds a listener.

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
    net::listener l = net::tcp::listen("127.0.0.1:0");
    l.close();
    println("{} {}", static_cast<bool>(none), static_cast<bool>(l));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](listener.md): a handle that holds none
- [is_closed](is_closed.md): whether the listener was closed
- [sgcl::net::listener](../listener.md)
