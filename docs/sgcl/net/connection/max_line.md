[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::max_line

```cpp
size_t max_line() const noexcept;
```

Returns the longest line [read_line](read_line.md) accepts, in bytes: 64 KB unless [set_max_line](set_max_line.md)
changed it, 0 for no bound.

## Parameters

None.

## Return value

The bound, in bytes.

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
    println("{}", a.max_line());
    a.set_max_line(1024);
    println("{}", a.max_line());
}
```

Output:

```text
65536
1024
```

## See also

- [set_max_line](set_max_line.md): sets the bound
- [sgcl::net::connection](README.md)
