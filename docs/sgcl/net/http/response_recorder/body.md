[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::body

```cpp
string body() const;
```

Returns everything the handler wrote to the body: what its flushes took and what is still buffered, a file it
wrote read in. None for a HEAD and for a status without a body (1xx, 204, 304), as the server would send none.

## Parameters

None.

## Return value

The body, as a string of bytes.

## Complexity

Linear in the size of the body.

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
    w.write("abc");
    w.flush();
    w.write("def");
    println("{} ({} flush)", rec.body(), rec.flushes());
}
```

Output:

```text
abcdef (1 flush)
```

## See also

- [response_writer::write](../response_writer/write.md)
- [sgcl::net::http::response_recorder](README.md)
