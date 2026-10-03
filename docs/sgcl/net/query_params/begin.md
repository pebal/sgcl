[sgcl](../../README.md) › [net](../README.md) › [query_params](README.md)

# sgcl::net::query_params::begin

```cpp
auto begin() const noexcept;
```

An iterator to the first pair. The pairs are `pair<string, string>`, the name and the value, unescaped and in their
order; they are read through the iterator, not changed.

## Parameters

None.

## Return value

A constant iterator to the first pair.

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
    net::query_params params("q=a+b&page=2");
    for (auto it = params.begin(); it != params.end(); ++it) {
        println("{} = {}", it->first, it->second);
    }
}
```

Output:

```text
q = a b
page = 2
```

## See also

- [end](end.md): the iterator past the last pair
- [sgcl::net::query_params](README.md)
