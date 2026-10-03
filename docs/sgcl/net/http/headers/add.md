[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::add

```cpp
headers& add(const string& name, const string& value) noexcept;
```

Appends a field `name: value` at the end of the list, Go's `Header.Add`: the fields of the same name already there
are kept, before it. The name and the value are kept as given and checked where they are written
([client](../client.md#rules), [response_writer](../response_writer.md)): a value with a line break makes the
client's send an error and a handler's response a 500, never a field more on the wire.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field, kept as written |
| `value` | the value |

## Return value

`*this`.

## Complexity

Constant, amortized.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Set-Cookie", "a=1").add("set-cookie", "b=2");
    h.add("X-Name", "line one\r\nInjected: yes");  // kept here, refused where it is written
    println("{} {}", h.size(), h.get_all("Set-Cookie"));
}
```

Output:

```text
3 ["a=1", "b=2"]
```

## See also

- [set](set.md): one field of the name, in the place of the first
- [get_all](get_all.md): every value of a name
- [sgcl::net::http::headers](../headers.md)
