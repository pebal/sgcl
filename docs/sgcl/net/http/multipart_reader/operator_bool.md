[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [multipart_reader](README.md)

# sgcl::net::http::multipart_reader::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the reader has a body: `false` for a default-constructed one, `true` for one made of a stream, whatever
became of its parse.

## Parameters

None.

## Return value

Whether there is a body.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::multipart_reader none;
    net::http::multipart_reader some(io::reader(make_tracked<io::buffer>("--b--\r\n")), "b");
    println("{} {}", (bool)none, (bool)some);
}
```

Output:

```text
false true
```

## See also

- [(constructor)](multipart_reader.md)
- [multipart_reader](README.md)
