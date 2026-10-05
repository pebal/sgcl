[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::http01_path

```cpp
static string http01_path(const string& token);
```

The path http-01 serves the key authorization at (RFC 8555 §8.3), on port 80 of the name: `/.well-known/acme-challenge/`
and the token.

## Parameters

| Parameter | Description |
|---|---|
| `token` | the challenge's token |

## Return value

The path.

## Complexity

Linear in the length of the token.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    println("{}", net::acme::client::http01_path("LoqXcYV8q5ONbJQxbmR7SCTNo3tiAXDfowyjxAjEuX0"));
}
```

Output:

```text
/.well-known/acme-challenge/LoqXcYV8q5ONbJQxbmR7SCTNo3tiAXDfowyjxAjEuX0
```

## See also

- [key_authorization](key_authorization.md)
- [manager::http_handler](../manager/http_handler.md): a handler that serves it
- [sgcl::net::acme::client](README.md)
