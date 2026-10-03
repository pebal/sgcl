[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::size

```cpp
size_t size() const noexcept;
```

The number of pairs, a name counted as often as it comes.

## Parameters

None.

## Return value

The number of pairs.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println(net::query_params("a=1&a=2&b=3").size());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md): whether there are none
- [sgcl::net::query_params](../query_params.md)
