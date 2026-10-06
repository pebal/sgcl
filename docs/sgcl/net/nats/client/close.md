[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::close

```cpp
expected<void, io::error> close() const noexcept;
```

The connection closed at once; the subscriptions end, what they received staying to be read. Publications still
buffered are dropped: [flush](flush.md) or [drain](drain.md) first to have them sent.

## Parameters

None.

## Return value

Nothing, or the close's error.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    nc.close();
    println("{}", bool(nc.publish("x", "y")));
}
```

Output:

```text
false
```

## See also

- [drain](drain.md)
- [client](README.md)
