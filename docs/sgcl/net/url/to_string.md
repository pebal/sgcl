[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::to_string

```cpp
string to_string() const noexcept;
```

The serialization of the URL, the standard's `href`, Go's `URL.String`: the string the `url` keeps, returned as it is.

## Parameters

None.

## Return value

The serialization.

## Complexity

Constant: a copy of the string.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println(net::url("HTTP://EXAMPLE.com:80/./a/%7e?x#y").to_string());
}
```

Output:

```text
http://example.com/a/%7e?x#y
```

## See also

- [parse](parse.md): the text read
- [sgcl::net::url](../url.md)
