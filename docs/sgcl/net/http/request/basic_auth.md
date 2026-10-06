[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::basic_auth

```cpp
optional<credentials> basic_auth() const noexcept;
```

The user and the password of the request's `Authorization: Basic`, Go's `r.BasicAuth`: what a handler of its own reads
when it does not go through [basic_auth](../basic_auth/README.md). The scheme without case, the user before the first
colon of the decoded text, the password after it.

## Parameters

None.

## Return value

The user and the password as [credentials](../credentials.md); `nullopt` for no `Authorization`, another scheme, or a value that is not base64 of a
text with a colon.

## Complexity

Linear in the field.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto req = net::http::test_request("GET", "/");
    req.set_header("Authorization", "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
    if (auto who = req.basic_auth()) {
        println("{} / {}", who->user, who->password);
    }
    println("{}", net::http::test_request("GET", "/").basic_auth().has_value());
}
```

Output:

```text
Aladdin / open sesame
false
```

## See also

- [set_basic_auth](set_basic_auth.md)
- [authenticated_user](authenticated_user.md)
- [request](README.md)
