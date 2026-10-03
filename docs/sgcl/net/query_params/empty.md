[sgcl](../../README.md) › [net](../README.md) › [query_params](README.md)

# sgcl::net::query_params::empty

```cpp
bool empty() const noexcept;
```

Checks whether there are no pairs.

## Parameters

None.

## Return value

`true` when there are no pairs.

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
    println("{} {} {}", net::query_params().empty(), net::query_params("?").empty(),
            net::query_params("a").empty());
}
```

Output:

```text
true true false
```

## See also

- [size](size.md): the number of pairs
- [sgcl::net::query_params](README.md)
