[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::operator==, operator\<=\> (sgcl::net::url)

```cpp
/*(1)*/ friend bool operator==(const url& a, const url& b) noexcept;
/*(2)*/ friend std::strong_ordering operator<=>(const url& a, const url& b) noexcept;
```

Compare two URLs by their serializations: two texts that parse to the same URL are equal, `HTTP://EXAMPLE.com:80/./a`
and `http://example.com/a`. `!=`, `<`, `<=`, `>` and `>=` are made of these by the compiler.

1. `true` when the serializations are the same.
2. The order of the serializations, byte by byte.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the URL on the left |
| `b` | the URL on the right |

## Return value

- (1) Whether the URLs are equal.
- (2) `std::strong_ordering::less`, `equal` or `greater`.

## Complexity

Linear in the length of the shorter serialization.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", net::url("HTTP://EXAMPLE.com:80/./a") == net::url("http://example.com/a"));
    println("{}", net::url("http://a.com/") < net::url("http://b.com/"));
}
```

Output:

```text
true
true
```

## See also

- [to_string](to_string.md): the serialization
- [sgcl::net::url](../url.md)
