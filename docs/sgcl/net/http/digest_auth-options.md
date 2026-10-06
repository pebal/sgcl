[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [digest_auth](digest_auth/README.md) › options

# sgcl::net::http::digest_auth::options

```cpp
#include "sgcl/net/http/auth.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class digest_auth {
    public:
        struct options {
            vector<algorithm> algorithms = {algorithm::sha256, algorithm::md5};
            bool auth_int = false;
            duration nonce_lifetime = std::chrono::minutes(5);
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::digest_auth::options` is what a [digest_auth](digest_auth/README.md) offers: a plain struct, its
fields set by name.

## Member objects

| Member | Description |
|---|---|
| `algorithms` | the challenges of a 401, one an algorithm, in this order ([digest_auth::algorithm](digest_auth-algorithm.md)); a client takes the first it has. SHA-256 and MD5 by default |
| `auth_int` | qop `auth-int` offered beside `auth`: the body in the hash, read whole before the handler; `false` by default |
| `nonce_lifetime` | how long a nonce is good; past it the right password gets `stale=true` and a new nonce. 5 minutes by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::digest_auth::options o;
    o.algorithms = {net::http::digest_auth::algorithm::sha256_sess};
    o.auth_int = true;
    net::http::server srv;
    srv.use(net::http::digest_auth("r", [](const string&) -> optional<string> { return nullopt; }, o));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("in"); });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    string challenge = rec.header("WWW-Authenticate");
    println("{} {}", challenge.contains("algorithm=SHA-256-sess"), challenge.contains("auth-int"));
}
```

Output:

```text
true true
```

## See also

- [digest_auth](digest_auth/README.md)
- [sgcl::net::http](README.md)
