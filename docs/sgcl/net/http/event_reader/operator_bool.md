[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_reader](README.md)

# sgcl::net::http::event_reader::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the reader has a stream: `false` for a default-constructed one.

## Parameters

None.

## Return value

Whether there is a stream.

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
    net::http::event_reader none;
    net::http::event_reader some(io::reader(make_tracked<io::buffer>("data: x\n\n")));
    println("{} {}", (bool)none, (bool)some);
}
```

Output:

```text
false true
```

## See also

- [(constructor)](event_reader.md)
- [sgcl::net::http::event_reader](README.md)
