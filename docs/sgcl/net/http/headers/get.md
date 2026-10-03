[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::get

```cpp
string get(const string& name) const noexcept;
```

Returns the value of the first field named `name`, found in any ASCII case, Go's `Header.Get`. A name that is not
there gives `""`, as a field with an empty value does: [contains](contains.md) tells the two apart. The value is made
into a string of its own; the list is not changed.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field, in any case |

## Return value

The value of the first field of the name, or `""` when there is none.

## Complexity

Linear in the number of fields, and in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Accept", "text/html").add("accept", "text/plain").add("X-Empty", "");
    println("[{}] [{}] [{}]", h.get("ACCEPT"), h.get("X-Empty"), h.get("X-Missing"));
}
```

Output:

```text
[text/html] [] []
```

## See also

- [get_all](get_all.md): every value of a name
- [contains](contains.md): whether the name is there at all
- [sgcl::net::http::headers](../headers.md)
