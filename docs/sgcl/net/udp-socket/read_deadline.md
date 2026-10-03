[sgcl](../../README.md) › [net](../README.md) › [udp](../udp.md) › [socket](../udp-socket.md)

# sgcl::net::udp::socket::read_deadline

```cpp
time_point read_deadline() const noexcept;
```

Returns the deadline of the receives, set by [set_read_deadline](set_read_deadline.md) or
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
    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    println("{}", s.read_deadline() == time_point());
    time_point t = clock::now() + 5s;
    s.set_read_deadline(t);
    println("{} {}", s.read_deadline() == t, s.write_deadline() == time_point());
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
- [sgcl::net::udp::socket](../udp-socket.md)
