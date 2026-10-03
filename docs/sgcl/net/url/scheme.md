[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::scheme

```cpp
string scheme() const noexcept;
```

The scheme without its `:`, lowercased: `https`; Go's `URL.Scheme`.

## Parameters

None.

## Return value

The scheme.

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
    println(net::url("HTTPS://example.com/").scheme());
    println(net::url("mailto:x@example.com").scheme());
}
```

Output:

```text
https
mailto
```

## See also

- [with_scheme](with_scheme.md): another scheme
- [is_special](is_special.md): whether the scheme is special
- [sgcl::net::url](../url.md)
