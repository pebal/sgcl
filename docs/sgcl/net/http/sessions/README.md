[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::sessions

```cpp
#include "sgcl/net/http/session.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class sessions;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

The sessions of a server's requests, a [middleware](../middleware.md): before the handler, each request's
[session](../session/README.md) is loaded from its cookie (a new one when it has none, or one past its time); the
handler reads and sets its values; as the response's head is made — at the first flush, or at the end — the session is
saved. Two stores: [in_cookie](in_cookie.md), the values in the cookie itself, sealed with XChaCha20-Poly1305 under a key
of the program's, so that the server keeps nothing and any of its processes reads them; and
[in_memory](in_memory.md), the values on the server by a random id of 128 bits the cookie carries. Go's standard library
has none (gorilla/sessions is the usual package, with the same two stores).

## Rules

- Secure defaults: the cookie `__Host-session`, `Secure`, `HttpOnly`, `SameSite=Lax`, `Path=/`, for 24 hours from the
  session's start. A `__Host-` cookie must be secure, of the path `/` and of no domain, and the options are checked for
  it: what a browser would refuse is `invalid_argument` when the middleware is made.
- A cookie is set only when something is to be kept: a session new and empty (an anonymous visitor's) sets none; one
  read and left as it was sets none again (but with an idle timeout, whose last use the sealed cookie carries).
- A session lives `max_age` from its start and `idle_timeout` from its last request; past either it is a new one.
- A handle of one word: copies share the store and the keys. The keys live in plain memory, zeroed when the store goes.

## Member types

| Type | Definition |
|---|---|
| [options](../sessions-options.md) | the cookie's name and attributes, the session's lifetime |

## Member functions

| Function | Description |
|---|---|
| [in_cookie](in_cookie.md) | the values sealed in the cookie |
| [in_memory](in_memory.md) | the values on the server, by an id |
| [size](size.md) | how many sessions the server's store holds |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::sessions::in_cookie(crypto::random::secret(32)));
    srv.route("POST /login", [](net::http::request req, net::http::response_writer w) {
        net::http::session s(req);
        s.renew();
        s.set("user", req.query("name"));
        w.write("welcome\n");
    });
    srv.route("GET /me", [](net::http::request req, net::http::response_writer w) {
        net::http::session s(req);
        w.write(s.is_new() ? string("nobody\n") : s.get("user") + "\n");
    });

    net::http::response_recorder login;
    login.serve(srv, net::http::test_request("POST", "/login?name=ann"));
    net::http::cookie login_cookie(login.header("Set-Cookie"));
    string cookie = login_cookie.name + "=" + login_cookie.value;   // the browser keeps it

    auto again = net::http::test_request("GET", "/me");
    again.set_header("Cookie", cookie);
    net::http::response_recorder me, stranger;
    me.serve(srv, again);
    stranger.serve(srv, net::http::test_request("GET", "/me"));
    print("{}{}", me.body(), stranger.body());
}
```

Output:

```text
ann
nobody
```

## See also

- [session](../session/README.md): the session of one request
- [sessions::options](../sessions-options.md)
- [csrf](../csrf/README.md): its synchronizer tokens live in the session
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
