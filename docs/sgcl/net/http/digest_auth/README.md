[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::digest_auth

```cpp
#include "sgcl/net/http/auth.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class digest_auth;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Digest authentication of a server (RFC 7616), a [middleware](../middleware.md): the password never crosses the network,
only a hash of it with a nonce of the server's, a nonce of the client's and the request's method and target. A request
without credentials, or with wrong ones, is answered 401 with one `WWW-Authenticate: Digest` challenge an algorithm
(SHA-256 and MD5 by default, in that order: MD5 for the clients written before 2015; SHA-512/256 and the `-sess` forms
when asked), `qop="auth"` (and `auth-int` when asked: the body hashed too), a nonce and an opaque value. A request whose
response is the one the user's password makes goes on to the handler, its user the request's
[authenticated_user](../request/authenticated_user.md). Go's standard library has no Digest; curl's `--digest`, Python's `HTTPDigestAuthHandler`
and the browsers are its clients, and so is this module's [client](../client/README.md).

## Rules

- A nonce is the time it was made and an HMAC-SHA256 of it under a key of the middleware's, so the server keeps none;
  past `nonce_lifetime` (5 minutes) it is stale, and the right password on a stale nonce gets a 401 with
  `stale=true`, which a client answers with the new nonce without asking for the password again.
- The count (`nc`) of a nonce must grow from request to request: a request seen once and sent again is refused. The
  counts of the nonces in use are kept, and dropped with their nonces.
- The uri of the credentials must be the request's target, the realm the middleware's, the algorithm and the qop ones
  it offered; the response is compared in constant time.
- With `auth_int`, a request that asks for it has its body read whole before the handler (within the server's
  `max_body_bytes`), hashed, and given to the handler from memory.
- The password callback gives the password of a user, or `nullopt` for none; it is called for every request.
- A handle of one word: copies share the settings and the nonces' key, which lives in plain memory.

## Member types

| Type | Definition |
|---|---|
| [algorithm](../digest_auth-algorithm.md) | MD5, SHA-256, SHA-512/256 and their `-sess` forms |
| [options](../digest_auth-options.md) | the algorithms offered, auth-int, the nonces' lifetime |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](digest_auth.md) | the middleware of a realm, a password function and the options |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::digest_auth("files", [](const string& user) -> optional<string> {
        if (user == "ann") {
            return string("secret");
        }
        return nullopt;
    }));
    srv.route("GET /me", [](net::http::request req, net::http::response_writer w) { w.write(req.authenticated_user() + "\n"); });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/me";

    net::http::client web;
    println("{}", web.get(url)->status());
    web.credentials = net::http::credentials{"ann", "secret"};
    print("{}", web.get(url)->text().value());
    srv.close();
}
```

Output:

```text
401
ann
```

## See also

- [basic_auth](../basic_auth/README.md)
- [client](../client/README.md): its `credentials` answer the challenge
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
