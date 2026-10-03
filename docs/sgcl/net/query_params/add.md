[sgcl](../../README.md) › [net](../README.md) › [query_params](README.md)

# sgcl::net::query_params::add

```cpp
expected<void, io::error> add(const string& name, const string& value) noexcept;
```

Adds the pair at the end, after any pair of the same name; Go's `Values.Add`. The pair is refused, and the pairs stay
as they were, when they would be written past [the limit](README.md#rules) of 512 MiB.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, unescaped |
| `value` | the value, unescaped |

## Return value

Nothing, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the operation
`add query pair` and the name, when the pairs would be written past 512 MiB (a byte the form escapes is three).

## Complexity

Linear in the length of `name` and `value`, whose written length is measured; amortized constant beside it.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params params;
    params.add("tag", "go");
    params.add("tag", "c++");
    params.add("q", "a b");
    println(params.to_string());
}
```

Output:

```text
tag=go&tag=c%2B%2B&q=a+b
```

## See also

- [set](set.md): one value for a name
- [sgcl::net::query_params](README.md)
