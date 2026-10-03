[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::get

```cpp
string get(const string& name) const noexcept;
```

The value of the first pair of the name; Go's `Values.Get`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |

## Return value

The value, or the empty string when there is no pair of the name ([contains](contains.md) tells that from an empty
value).

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
    net::query_params params("tag=go&tag=c%2B%2B&empty=");
    println("[{}] [{}] [{}]", params.get("tag"), params.get("empty"), params.get("none"));
}
```

Output:

```text
[go] [] []
```

## See also

- [get_all](get_all.md): every value
- [contains](contains.md): whether the name is there
- [sgcl::net::query_params](../query_params.md)
