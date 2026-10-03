[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::request_target

```cpp
string request_target() const noexcept;
```

What goes into the request line of HTTP: the path, and `?` and the query when there is one, without the fragment. Go's
`URL.RequestURI()`.

## Parameters

None.

## Return value

The path and the query.

## Complexity

Linear in the length of the part.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println(net::url("https://example.com/a b?q=1#top").request_target());
    println(net::url("https://example.com").request_target());
}
```

Output:

```text
/a%20b?q=1
/
```

## See also

- [path](path.md), [query](query.md): the two parts
- [sgcl::net::url](README.md)
