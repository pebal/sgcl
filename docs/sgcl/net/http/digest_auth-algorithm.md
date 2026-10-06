[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [digest_auth](digest_auth/README.md) › algorithm

# sgcl::net::http::digest_auth::algorithm

```cpp
#include "sgcl/net/http/auth.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class digest_auth {
    public:
        enum class algorithm : uint8_t {
            md5,
            md5_sess,
            sha256,
            sha256_sess,
            sha512_256,
            sha512_256_sess,
        };
    };
}
```

The hash algorithms of Digest (RFC 7616 §3.2, §6.1) a [digest_auth](digest_auth/README.md) offers. A `-sess` form hashes
the password with the server's and the client's nonces once, at the first request of a nonce, so a client may keep
that hash rather than the password. MD5 is broken as a hash; RFC 7616 keeps it for the clients of RFC 2617, which know
no other.

| Value | Description |
|---|---|
| `md5` | `MD5`: the clients of RFC 2617, browsers before 2015 |
| `md5_sess` | `MD5-sess` |
| `sha256` | `SHA-256`: curl, Python, browsers of today |
| `sha256_sess` | `SHA-256-sess` |
| `sha512_256` | `SHA-512-256`: SHA-512 cut to 256 bits (FIPS 180-4), curl and few others |
| `sha512_256_sess` | `SHA-512-256-sess` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    using algorithm = net::http::digest_auth::algorithm;
    net::http::server srv;
    srv.use(net::http::digest_auth("r", [](const string&) -> optional<string> { return nullopt; },
                                   {.algorithms = {algorithm::sha512_256, algorithm::sha256, algorithm::md5}}));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("in"); });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    println("{} challenges", rec.headers().get_all("WWW-Authenticate").size());
}
```

Output:

```text
3 challenges
```

## See also

- [digest_auth::options](digest_auth-options.md)
- [digest_auth](digest_auth/README.md)
