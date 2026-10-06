[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::renew

```cpp
void renew() noexcept;
```

A new identity for the same values, after a login: an id an attacker planted in a victim's browser before the login is
worth nothing after it (session fixation). In an [in_memory](../sessions/in_memory.md) store the values move to a new id
and the old one is dropped; a cookie of [in_cookie](../sessions/in_cookie.md) is sealed anew. Undoes a
[destroy](destroy.md) of the same request.

## Parameters

None.

## Return value

None.

## Complexity

Constant; the store does the rest when the session is saved.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto store = net::http::sessions::in_memory();
    net::http::server srv;
    srv.use(store);
    srv.route("POST /login", [](net::http::request req, net::http::response_writer w) {
        net::http::session s(req);
        s.renew();
        s.set("user", "ann");
    });
    net::http::response_recorder first;
    first.serve(srv, net::http::test_request("POST", "/login"));
    net::http::cookie first_cookie(first.header("Set-Cookie"));
    string before = first_cookie.name + "=" + first_cookie.value;
    auto again = net::http::test_request("POST", "/login");
    again.set_header("Cookie", before);
    net::http::response_recorder second;
    second.serve(srv, again);
    net::http::cookie second_cookie(second.header("Set-Cookie"));
    string after = second_cookie.name + "=" + second_cookie.value;
    println("{} {}", before != after, store.size());
}
```

Output:

```text
true 1
```

## See also

- [destroy](destroy.md)
- [session](README.md)
