[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::contains

```cpp
bool contains(const string& name) const noexcept;
```

Checks whether a pair of the name is there, with any value, an empty one among them; Go's `Values.Has`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |

## Return value

`true` when there is a pair of the name.

## Complexity

Linear in the number of pairs.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params params("debug&page=2");
    println("{} {} {}", params.contains("debug"), params.contains("page"), params.contains("lang"));
}
```

Output:

```text
true true false
```

## See also

- [get](get.md): the value
- [sgcl::net::query_params](../query_params.md)
