[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::read_deadline

```cpp
time_point read_deadline() const noexcept;
```

Returns the deadline of the reads, set by [set_read_deadline](set_read_deadline.md) or
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
    println("{}", a.read_deadline() == time_point());
    time_point t = clock::now() + 5s;
    a.set_deadline(t);
    println("{} {}", a.read_deadline() == t, a.write_deadline() == t);
}
```

Output:

```text
true
true true
```

## See also

- [write_deadline](write_deadline.md): the other direction
- [set_read_deadline](set_read_deadline.md): sets it
- [sgcl::net::connection](README.md)
