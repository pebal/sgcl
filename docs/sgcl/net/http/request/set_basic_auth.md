[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::set_basic_auth

```cpp
request& set_basic_auth(const string& user, const string& password) noexcept;
```

Sets `Authorization: Basic` of the user and the password (RFC 7617: base64 of `user:password`), sent with the request
whatever the server asks, Go's `SetBasicAuth`. The password is in clear text: over https alone. The field follows a
redirect to the same host or one under it, and is dropped on the way to another, as every `Authorization` is; a client's
[credentials](../credentials.md) are sent only when a server asks.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user, without a colon (RFC 7617 §2: the first colon parts the two) |
| `password` | the password |

## Return value

`*this`.

## Complexity

Linear in the lengths.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::request req("GET", "https://api.example/v1/status");
    req.set_basic_auth("Aladdin", "open sesame");
    println("{}", req.header("Authorization"));
}
```

Output:

```text
Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
```

## See also

- [basic_auth](basic_auth.md): the field read back
- [set_credentials](set_credentials.md): sent only when asked
- [request](README.md)
