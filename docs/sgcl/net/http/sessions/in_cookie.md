[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [sessions](README.md)

# sgcl::net::http::sessions::in_cookie

```cpp
static sessions in_cookie(const crypto::secret_bytes& key);                                         // (1)
static sessions in_cookie(const crypto::secret_bytes& key, const options& o);                       // (2)
static sessions in_cookie(const crypto::secret_bytes& key, const crypto::secret_bytes& previous,    // (3)
                          const options& o);
```

Sessions whose values travel in the cookie, sealed: the values, the session's start and its last use packed and sealed
with XChaCha20-Poly1305 under `key` (a random nonce of 24 bytes each time, the cookie's name as associated data, so
that a sealed value moved to another cookie does not open), base64url without padding. A cookie that does not open, or
whose times are past the options' lifetime, is a new session. The server keeps nothing: any of its processes with the
key reads the session, and a session cannot be ended before its time but by the client dropping its cookie (a
[destroy](../session/destroy.md) expires it in the browser; a copy kept elsewhere stays valid to its time, which
[in_memory](in_memory.md) does not allow).

1. The default options.
2. The options given.
3. Cookies sealed under `previous` open too, and are sealed under `key` when saved: a key rotated without ending every
   session at once.

A sealed session past about 4 KB is not set: browsers keep 4096 bytes of a cookie, its name and attributes included;
an error goes to slog and the values of that response are lost. The values of a cookie session are a few small
strings.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key that seals, 32 bytes (`crypto::random::secret(32)`, or one read with `crypto::read_secret`) |
| `previous` | a key that only opens, 32 bytes |
| `o` | the cookie and the lifetime ([sessions::options](../sessions-options.md)) |

## Return value

The middleware.

## Complexity

Constant. Each request then opens its cookie and, when it changed, seals it: linear in the values.

## Exceptions

`invalid_argument` for a key of another length, and for options a browser would refuse (a `__Host-` cookie that is
not secure, has a domain or a path other than `/`; `SameSite=None` without `Secure`; a name that is not a token).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto old_key = crypto::random::secret(32);
    auto new_key = crypto::random::secret(32);
    auto counter = [](net::http::request req, net::http::response_writer w) {
        net::http::session s(req);
        int n = s.contains("n") ? std::stoi(s.get("n").str()) + 1 : 1;
        s.set("n", to_string(n));
        w.write(to_string(n));
    };
    net::http::server before, after;
    before.use(net::http::sessions::in_cookie(old_key));
    before.route("GET /", counter);
    after.use(net::http::sessions::in_cookie(new_key, old_key, {}));
    after.route("GET /", counter);

    net::http::response_recorder first;
    first.serve(before, net::http::test_request("GET", "/"));
    auto req = net::http::test_request("GET", "/");
    net::http::cookie first_cookie(first.header("Set-Cookie"));
    req.set_header("Cookie", first_cookie.name + "=" + first_cookie.value);
    net::http::response_recorder second;
    second.serve(after, req);
    println("{} {}", first.body(), second.body());
}
```

Output:

```text
1 2
```

## See also

- [in_memory](in_memory.md)
- [sessions](README.md)
