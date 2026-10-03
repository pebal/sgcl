[sgcl](../../README.md) › [net](../README.md) › [listener](../listener.md)

# sgcl::net::operator==, operator!= (sgcl::net::listener)

```cpp
friend bool operator==(const listener& a, const listener& b) noexcept;
```

Checks whether `a` and `b` are handles of the same listener: copies of one handle, not two listeners on one port.
Two handles that hold none are equal. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same listener, or both none; `false` otherwise.

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
    net::listener copy = l;
    net::listener other = net::tcp::listen("127.0.0.1:0");
    println("{} {} {}", l == copy, l == other, l != other);
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](listener.md): a copy that is the same listener
- [sgcl::net::listener](../listener.md)
