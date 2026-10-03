[sgcl](../../README.md) › [net](../README.md) › [udp](../udp.md) › [socket](../udp-socket.md)

# sgcl::net::udp::socket::write_deadline

```cpp
time_point write_deadline() const noexcept;
```

Returns the deadline of the sends, set by [set_write_deadline](set_write_deadline.md) or
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
    time_point t = clock::now() + 5s;
    s.set_deadline(t);
    println("{} {}", s.write_deadline() == t, s.read_deadline() == t);
}
```

Output:

```text
true true
```

## See also

- [read_deadline](read_deadline.md): the other direction
- [set_write_deadline](set_write_deadline.md): sets it
- [sgcl::net::udp::socket](../udp-socket.md)
