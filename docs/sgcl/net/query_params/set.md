[sgcl](../../README.md) › [net](../README.md) › [query_params](README.md)

# sgcl::net::query_params::set

```cpp
expected<void, io::error> set(const string& name, const string& value) noexcept;
```

Sets the value of the name, Go's `Values.Set` and the standard's `set`: the first pair of the name takes the value and
the other pairs of the name go, so the name keeps its place; a pair is added at the end when there was none. The value
is refused, and the pairs stay as they were, when they would be written past [the
limit](README.md#rules) of 512 MiB.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, unescaped |
| `value` | the value, unescaped |

## Return value

Nothing, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the operation
`set query pair` and the name, when the pairs would be written past 512 MiB (a byte the form escapes is three).

## Complexity

Linear in the number of pairs and in the length of the name, of `value` and of the values of the name, whose written
length is measured.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params params("page=1&q=go&page=7");
    params.set("page", "2");
    params.set("lang", "pl");
    println(params.to_string());
}
```

Output:

```text
page=2&q=go&lang=pl
```

## See also

- [add](add.md): another pair of a name
- [erase](erase.md): no pair of a name
- [sgcl::net::query_params](README.md)
