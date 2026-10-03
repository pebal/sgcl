[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::write_deadline

```cpp
time_point write_deadline() const noexcept;
```

Returns the deadline of the writes, set by [set_write_deadline](set_write_deadline.md) or
[set_deadline](set_deadline.md); `time_point()` when there is none.

## Parameters

None.

## Return value

The deadline, or `time_point()`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    auto [a, b] = net::connection::in_memory();
    a.set_write_deadline(clock::now() + 5s);
    println("{} {}", a.write_deadline() != time_point(), a.read_deadline() != time_point());
    a.set_write_deadline(time_point());
    println("{}", a.write_deadline() == time_point());
}
```

Output:

```text
true false
true
```

## See also

- [read_deadline](read_deadline.md): the other direction
- [set_write_deadline](set_write_deadline.md): sets it
- [sgcl::net::connection](README.md)
