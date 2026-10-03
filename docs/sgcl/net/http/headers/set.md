[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](README.md)

# sgcl::net::http::headers::set

```cpp
headers& set(const string& name, const string& value) noexcept;
```

Sets the field `name` to `value`, Go's `Header.Set`: the value takes the place of the first field of the name, found
in any ASCII case, and the other fields of the name are dropped; a name that is not there is appended. The first
field keeps its name as it was written; the others keep their order. The name and the value are kept as given and
checked where they are written ([client](../client/README.md#rules), [response_writer](../response_writer/README.md)).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field, in any case |
| `value` | the value |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Accept", "text/html").add("Host", "example.com").add("accept", "text/plain");
    h.set("ACCEPT", "application/json").set("X-Request-Id", "42");
    for (auto [name, value] : h) {
        println("{}: {}", name, value);
    }
}
```

Output:

```text
Accept: application/json
Host: example.com
X-Request-Id: 42
```

## See also

- [add](add.md): a field appended, the others kept
- [erase](erase.md): every field of a name dropped
- [sgcl::net::http::headers](README.md)
