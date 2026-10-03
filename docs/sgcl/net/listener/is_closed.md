[sgcl](../../README.md) › [net](../README.md) › [listener](../listener.md)

# sgcl::net::listener::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the listener was closed, by [close](close.md) on this handle or on any copy.

## Parameters

None.

## Return value

`true` after `close`, `false` before it.

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
    net::listener l = net::tcp::listen("127.0.0.1:0");
    println("{}", l.is_closed());
    l.close();
    println("{}", l.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md): stops listening
- [sgcl::net::listener](../listener.md)
