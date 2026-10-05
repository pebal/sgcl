[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_source](README.md)

# sgcl::net::http::event_source::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle has a source: `false` for a default-constructed one, `true` for one made of a URL, closed or not.

## Parameters

None.

## Return value

Whether there is a source.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::event_source none;
    net::http::event_source some("http://127.0.0.1:1/events");
    println("{} {}", (bool)none, (bool)some);
}
```

Output:

```text
false true
```

## See also

- [(constructor)](event_source.md)
- [sgcl::net::http::event_source](README.md)
