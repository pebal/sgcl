[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [digest_auth](README.md)

# sgcl::net::http::digest_auth::digest_auth

```cpp
digest_auth(const string& realm, function<optional<string>(const string& user)> password);    // (1)
digest_auth(const string& realm, function<optional<string>(const string& user)> password,     // (2)
            const options& o);
```

1. SHA-256 and MD5 offered, qop auth, nonces of 5 minutes.
2. The algorithms, auth-int and the nonces' lifetime of the options ([digest_auth::options](../digest_auth-options.md)).

- (1–2) The middleware of a realm, the name in the challenges and in the hash, and of the function that gives the
  password of a user (`nullopt` for a user who is not one). A key of 256 bits for the nonces and an opaque value are
  drawn now.

## Parameters

| Parameter | Description |
|---|---|
| `realm` | the protection space's name |
| `password` | the password of a user, or `nullopt` |
| `o` | the algorithms, auth-int, the nonces' lifetime |

## Complexity

Constant.

## Exceptions

- (1) What the move of `password` throws.
- (2) `invalid_argument` for options without an algorithm; what the move of `password` throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::digest_auth strict("vault", [](const string&) -> optional<string> { return nullopt; },
                                  {.algorithms = {net::http::digest_auth::algorithm::sha512_256}});
    net::http::server srv;
    srv.use(strict);
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("in"); });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    string challenge = rec.header("WWW-Authenticate");
    println("{} {}", rec.status(), challenge.contains("algorithm=SHA-512-256"));
}
```

Output:

```text
401 true
```

## See also

- [digest_auth](README.md)
