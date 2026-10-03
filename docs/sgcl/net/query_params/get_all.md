[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::get_all

```cpp
vector<string> get_all(const string& name) const noexcept;
```

Every value of the name, in the order of their pairs; Go's `Values[name]`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |

## Return value

The values; empty when there is no pair of the name.

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
    net::query_params params("tag=go&page=1&tag=c%2B%2B");
    for (const string& tag : params.get_all("tag")) {
        println(tag);
    }
}
```

Output:

```text
go
c++
```

## See also

- [get](get.md): the first value
- [sgcl::net::query_params](../query_params.md)
