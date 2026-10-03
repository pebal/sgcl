[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::operator==, operator!= (sgcl::net::connection)

```cpp
friend bool operator==(const connection& a, const connection& b) noexcept;
```

Checks whether `a` and `b` are handles of the same connection: copies of one handle, not two connections to one
address. Two handles that hold none are equal. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same connection, or both none; `false` otherwise.

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
    auto [a, b] = net::connection::in_memory();
    net::connection copy = a;
    println("{} {} {}", a == copy, a == b, a != b);
    println("{}", net::connection() == net::connection());
}
```

Output:

```text
true false true
true
```

## See also

- [(constructor)](connection.md): a copy that is the same connection
- [sgcl::net::connection](README.md)
