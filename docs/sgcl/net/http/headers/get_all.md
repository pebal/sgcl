[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::get_all

```cpp
vector<string> get_all(const string& name) const noexcept;
```

Returns the value of every field named `name`, found in any ASCII case, in the order of the list, Go's
`Header.Values`: the fields that come more than once, `Set-Cookie` above all. A value that holds a list of its own
(`Accept: a, b`) is one value; it is not split at its commas.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the fields, in any case |

## Return value

The values in their order; empty when there is no field of the name.

## Complexity

Linear in the number of fields, and in the size of the values.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Set-Cookie", "a=1").add("Vary", "Accept, Origin").add("set-cookie", "b=2");
    println("{}", h.get_all("Set-Cookie"));
    println("{}", h.get_all("Vary"));
    println("{}", h.get_all("Allow").size());
}
```

Output:

```text
["a=1", "b=2"]
["Accept, Origin"]
0
```

## See also

- [get](get.md): the first value
- [add](add.md): a field more of the same name
- [sgcl::net::http::headers](../headers.md)
