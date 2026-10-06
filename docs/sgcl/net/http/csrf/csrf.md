[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [csrf](README.md)

# sgcl::net::http::csrf::csrf

```cpp
csrf();                                                     // (1)
explicit csrf(const options& o);                            // (2)
csrf(const crypto::secret_bytes& key, const options& o);    // (3)
```

1. The origin check alone, no origin trusted.
2. The options; double-submit cookies signed under a key drawn now, so that they do not outlive the process, nor pass to
   another process of the same site.
3. The options; double-submit cookies signed under `key` (HMAC-SHA256), for several processes behind one name. The key
   is copied into plain memory, zeroed when the middleware goes.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the double-submit cookies' signature; 32 bytes or more advised |
| `o` | the kind of tokens, the trusted origins, the names, the refusal ([csrf::options](../csrf-options.md)) |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto key = crypto::random::secret(32);
    net::http::csrf::options o;
    o.kind = net::http::csrf::tokens::double_submit;
    net::http::server a, b;
    for (auto* s : {&a, &b}) {
        s->use(net::http::csrf(key, o));
        s->route("GET /form", [](net::http::request req, net::http::response_writer w) {
            w.write(net::http::csrf::token(req));
        });
        s->route("POST /act", [](net::http::request, net::http::response_writer w) { w.write("done"); });
    }
    net::http::response_recorder page;
    page.serve(a, net::http::test_request("GET", "/form"));
    auto post = net::http::test_request("POST", "/act");
    net::http::cookie page_cookie(page.header("Set-Cookie"));
    post.set_header("Cookie", page_cookie.name + "=" + page_cookie.value);
    post.set_header("X-CSRF-Token", page.body());
    net::http::response_recorder other;
    other.serve(b, post);
    println("{}", other.body());
}
```

Output:

```text
done
```

## See also

- [csrf](README.md)
