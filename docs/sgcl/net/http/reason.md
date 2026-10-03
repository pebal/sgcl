[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::reason

```cpp
#include "sgcl/net/http/status.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    constexpr const char* reason(int code) noexcept;
}
```

The reason phrase of a status code as the IANA registry names it (RFC 9110 §15 and the RFCs it lists), Go's
`http.StatusText`: `"Not Found"` for 404, `"I'm a teapot"` for 418. A code the registry does not have gives `""`.
The phrase is a literal of static storage, usable in a constant expression.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the status code |

## Return value

The phrase, or `""` for a code the registry does not have.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    for (int code : {net::http::status::ok, net::http::status::not_found, 418, 599}) {
        println("{} \"{}\"", code, net::http::reason(code));
    }
}
```

Output:

```text
200 "OK"
404 "Not Found"
418 "I'm a teapot"
599 ""
```

## See also

- [status](status.md): the codes as constants
- [sgcl::net::http](README.md)
