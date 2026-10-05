[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::status

```cpp
int status() const noexcept;
```

Returns the status the handler wrote: the one of the head at the first flush, or the one set since when nothing was
flushed; 200 unless set.

## Parameters

None.

## Return value

The status.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::response_recorder rec;
    auto w = rec.writer();
    println("{}", rec.status());
    w.set_status(net::http::status::accepted);
    println("{}", rec.status());
    w.error(net::http::status::conflict);
    println("{}", rec.status());
}
```

Output:

```text
200
202
409
```

## See also

- [response_writer::set_status](../response_writer/set_status.md)
- [sgcl::net::http::response_recorder](README.md)
