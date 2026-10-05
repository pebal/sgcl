[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [event_reader](README.md)

# sgcl::net::http::event_reader::retry

```cpp
optional<duration> retry() const noexcept;
```

The reconnection time the stream set last, its last `retry` field (milliseconds, ASCII digits alone); none when it set
none.

## Parameters

None.

## Return value

The reconnection time, or none.

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
    net::http::event_reader events(io::reader(make_tracked<io::buffer>("data: a\n\nretry: 1500\ndata: b\n\n")));
    (void)events.next();
    println("{}", events.retry() ? events.retry()->to_string() : string("none"));
    (void)events.next();
    println("{}", events.retry()->to_string());
}
```

Output:

```text
none
1.5s
```

## See also

- [event_source](../event_source/README.md): what heeds it
- [sgcl::net::http::event_reader](README.md)
