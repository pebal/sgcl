[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::erase

```cpp
query_params& erase(const string& name) noexcept;
```

Removes every pair of the name, keeping the order of the others; Go's `Values.Del`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |

## Return value

`*this`.

## Complexity

Linear in the number of pairs and in the length of the pairs removed, whose written length is taken off the length
tracked.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params params("a=1&tag=go&b=2&tag=c");
    params.erase("tag").erase("none");
    println(params.to_string());
}
```

Output:

```text
a=1&b=2
```

## See also

- [set](set.md): one value for a name
- [sgcl::net::query_params](../query_params.md)
