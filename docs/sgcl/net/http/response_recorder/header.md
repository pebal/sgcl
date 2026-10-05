[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::header

```cpp
string header(const string& name) const noexcept;
```

Returns the first value of the field `name` among [headers](headers.md), the name compared without case; an empty
string when there is none.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field |

## Return value

The value, or an empty string.

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
    net::http::response_recorder rec;
    rec.writer().set_header("Content-Type", "application/json");
    println("{} [{}]", rec.header("content-type"), rec.header("X-None"));
}
```

Output:

```text
application/json []
```

## See also

- [headers](headers.md): all of them
- [sgcl::net::http::response_recorder](README.md)
