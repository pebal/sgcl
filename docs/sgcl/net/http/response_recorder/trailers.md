[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::trailers

```cpp
http::headers trailers() const noexcept;
```

Returns the trailers the handler set ([response_writer::trailers](../response_writer/trailers.md)), as they stand.

## Parameters

None.

## Return value

A copy of the trailers.

## Complexity

Linear in their number.

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
    w.write("data");
    w.trailers().set("X-Checksum", "e3b0c442");
    println("{}", rec.trailers().get("X-Checksum"));
}
```

Output:

```text
e3b0c442
```

## See also

- [response_writer::trailers](../response_writer/trailers.md)
- [sgcl::net::http::response_recorder](README.md)
